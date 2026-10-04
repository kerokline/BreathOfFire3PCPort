# Group R1A: party-member states 4, 6, 8 and party sets 0..2's field actions

**Status:** MEASURED (2026-10-04) - round fourteen
([`takeover-queue-round14.md`](takeover-queue-round14.md)), wave one, on the
round branch's tip `ba2c3c3`. **49 functions ours** (`src/game/rest_1a.cpp`,
declarations in `src/game/rest_1a.h`, the group's addresses in
`src/game/rest_1a_callees.h`, shadow name `rest_1a`): the cut's 49 rows for
R1A (`analysis/round14_cut.tsv`), each read to its last instruction with
capstone and fuzzed through the scenario harness in field mode
([`scenario_harness.md`](scenario_harness.md) section 7), used unchanged:
196,000 rounds, 0 mismatches. 85 controls planted (section 6): 77 refused
by a count, 3 equivalent mutants with their near variants refused, 4
refused by a fault only and replaced by in-table variants that are refused,
1 (C74) not refused first - the fuzz's fault, fixed. 22 state tables named
(section 2). No recorded route enters any of the 49 (section 9): fuzz only.

The band `0x51BA80..0x51D70C` is one thing: the field core's party-member
states 4, 6 and 8 (`Member_States` `0x65F960`, `Field_MemberFrame`'s by +1)
and the field actions of party sets 0, 1 and 2 - the dispatchers that reach
them through `Field_ActionBySet` `0x6609D0` and `Field_FormActions`
`0x660A44` (indexed by the loaded party set `0x90412C & 0x7F`), then by the
form (u16 `+0x2C`), the state (`+2`) and for set 2's form 2 the step (`+3`),
and the states those tables hold. Sets 0, 1 and 2 repeat one block of code:
`0x51BFD0` / `0x51C7C0` / `0x51CD10`, `0x51C190` / `0x51C980` / `0x51CED0`
and `0x51C270` / `0x51CA60` / `0x51CFB0` are byte-identical but for their
relative calls (capstone, normalised), and identical to set 5's
`PartyAction5_Form0Begin`, `_Form0Resolve` and `Field_CellPickup`
`0x51E930..0x51ECEE` (ours since round eight, `field_hidden.cpp`).

**Every start of the cut's 49 is a function** - none a case, a shared tail or
data; each ends in its own `ret` (or a tail `jmp`: `0x51BAA0` to
`Member_Follow`, and the 25 dispatchers' `jmp [table + index * 4]`). The
band tool reads 5,949 bytes against the cut's 6,423 (41 extents differ by the
padding after the `ret` only; 0 by code); the tool's extents are right, read
by hand. No code in the band is in no group: the gaps between the 49 are
`nop` padding, and the starts between them (`0x51BA60`, `0x51BBD0`,
`0x51BDA0`, `0x51C390`, `0x51C6A0`) are ours already. The band tool's "code
no list has": none.

| Function | Entry | Bytes | Reached through |
|---|---|--:|---|
| `Member_ResumeUnlessHeld800` | `0x51BA80` | 0x14 | `Member_States`[4] |
| `Member_FormActionState` | `0x51BAA0` | 0x125 | `Member_States`[6] |
| `Member_JumpState` | `0x51BCF0` | 0x12 | `Member_States`[8] |
| `Member_JumpAir` | `0x51BD10` | 0x8D | `Member_JumpSteps`[2] |
| `PartyFormAction0_Form0` | `0x51BE90` | 0x12 | `PartyFormAction0_Forms`[0] |
| `PartyFormAction_Form0Begin` | `0x51BEB0` | 0xFC | 7 cells (form-0 form-action states [0]) |
| `PartyAction0_Form0` | `0x51BFB0` | 0x12 | `PartyAction0_Forms`[0] |
| `PartyAction0_Form0Begin` | `0x51BFD0` | 0x1B1 | `PartyAction0_Form0States`[0] |
| `PartyAction0_Form0Resolve` | `0x51C190` | 0xDB | `PartyAction0_Form0States`[1] |
| `PartyAction0_CellPickup` | `0x51C270` | 0x11F | `E8` from `0x51C190` (3 sites) |
| `PartyFormAction0_Form1` | `0x51C430` | 0x12 | `PartyFormAction0_Forms`[1] |
| `PartyAction0_Form1` | `0x51C450` | 0x12 | `PartyAction0_Forms`[1] |
| `PartyFormAction0_Form2` | `0x51C470` | 0x12 | `PartyFormAction0_Forms`[2] |
| `PartyFormAction_Form2Turn` | `0x51C490` | 0x7E | 14 cells |
| `PartyAction0_Form2` | `0x51C510` | 0x12 | `PartyAction0_Forms`[2] |
| `PartyAction0_Form2Begin` | `0x51C530` | 0x166 | `PartyAction0_Form2States`[0] |
| `PartyFormAction0_ByForm` | `0x51C740` | 0x13 | `Field_FormActions`[0] |
| `PartyAction0_ByForm` | `0x51C760` | 0x13 | `Field_ActionBySet`[0] |
| `PartyFormAction1_Form0` | `0x51C780` | 0x12 | `PartyFormAction1_Forms`[0] |
| `PartyAction1_Form0` | `0x51C7A0` | 0x12 | `PartyAction1_Forms`[0] |
| `PartyAction1_Form0Begin` | `0x51C7C0` | 0x1B1 | `PartyAction1_Form0States`[0] |
| `PartyAction1_Form0Resolve` | `0x51C980` | 0xDB | `PartyAction1_Form0States`[1] |
| `PartyAction1_CellPickup` | `0x51CA60` | 0x11F | `E8` from `0x51C980` (3) |
| `PartyFormAction1_Form1` | `0x51CB80` | 0x12 | `PartyFormAction1_Forms`[1] |
| `PartyAction1_Form1` | `0x51CBA0` | 0x12 | `PartyAction1_Forms`[1] |
| `PartyFormAction1_Form2` | `0x51CBC0` | 0x12 | `PartyFormAction1_Forms`[2] |
| `PartyAction1_Form2` | `0x51CBE0` | 0x12 | `PartyAction1_Forms`[2] |
| `PartyFormAction1_ByForm` | `0x51CC00` | 0x13 | `Field_FormActions`[1] |
| `PartyAction1_ByForm` | `0x51CC20` | 0x13 | `Field_ActionBySet`[1] |
| `PartyFormAction2_Form0` | `0x51CC40` | 0x12 | `0x65FAC8`[0] (R1B's `0x51D710`) |
| `PartyFormAction_Form0Turn` | `0x51CC60` | 0x83 | 7 cells (form-0 form-action states [1]) |
| `PartyAction2_Form0` | `0x51CCF0` | 0x12 | `0x65FAD4`[0] (R1B's `0x51D730`) |
| `PartyAction2_Form0Begin` | `0x51CD10` | 0x1B1 | `PartyAction2_Form0States`[0] |
| `PartyAction2_Form0Resolve` | `0x51CED0` | 0xDB | `PartyAction2_Form0States`[1] |
| `PartyAction2_CellPickup` | `0x51CFB0` | 0x11F | `E8` from `0x51CED0` (3) |
| `PartyFormAction2_Form1` | `0x51D0D0` | 0x12 | `0x65FAC8`[1] |
| `PartyFormAction_Form1Begin` | `0x51D0F0` | 0x70 | form-1 form-action states [0], sets 0..2 |
| `PartyFormAction_Form1Turn` | `0x51D160` | 0x7C | form-1 form-action states [1], sets 0..2 |
| `PartyAction2_Form1` | `0x51D1E0` | 0x12 | `0x65FAD4`[1] |
| `PartyFormAction2_Form2` | `0x51D200` | 0x12 | `0x65FAC8`[2] |
| `PartyAction2_Form2` | `0x51D220` | 0x12 | `0x65FAD4`[2] |
| `PartyAction2_Form2State0` | `0x51D240` | 0x12 | `PartyAction2_Form2States`[0] |
| `PartyAction2_Form2Aim` | `0x51D260` | 0x87 | `PartyAction2_Form2State0Steps`[0] |
| `PartyAction2_Form2Strike` | `0x51D2F0` | 0x14C | `PartyAction2_Form2State0Steps`[1] |
| `PartyAction_FinishPalette` | `0x51D440` | 0x94 | 9 cells |
| `PartyAction2_CellStrike` | `0x51D4E0` | 0x188 | `E8` from `0x51D2F0` (3) |
| `PartyAction2_Form2State1` | `0x51D670` | 0x12 | `PartyAction2_Form2States`[1] |
| `PartyAction2_Form2Reaim` | `0x51D690` | 0x35 | `PartyAction2_Form2State1Steps`[0] |
| `PartyAction_WaitEffect` | `0x51D6D0` | 0x3D | 7 cells |

"Cells" are the `.data` dwords `band_rows.py --byte-tables` finds holding the
address (state tables of sets 0..5 from `0x65F9B4` to `0x6600D4`); the
`[k]` entries are by hand. The names are this group's, from what the code
does and the table that reaches it; "PartyAction" follows the sets' existing
names (`PartyAction5_ByForm`, `_Form0`, `_Form0Begin` ...) for the
`Field_ActionBySet` side, "PartyFormAction" the `Field_FormActions` side. What
a form, a form action or a set is in play is not read here. Only `0x51D440`
has a PSX twin (`analysis/pairs_propagated.json`, by callers, nine PLP-overlay
addresses, `0x801CEA04` among them); the sibling names none of them, so no
name transferred.

## 1. What each function does

`Sprite_Current` ("the sprite") is the object whose state runs: a party
member's `ObjTrio` record for the three member states, the leader's or a
member's for the actions. Its bytes: `+1` state, `+2` sub-state, `+3` step,
`+4` pose, `+5` member, `+6` the member it follows (or an effect index),
`+8` direction, `+9` / `+0xA` counters, `+0xB` a step or an index, `+0x2B`
a flag, u16 `+0x2C` the form (and a sound base), 16.16 `+0x34` / `+0x38`
position, s16 `+0x3E` height. "A step ahead" is a row of
`Field_DirectionSteps` read in place with the direction byte unmasked;
"the pose p" is `Sprite_EnsureAnimation(p)`; "(d - 1) / 2" is the signed
division the code makes (`dec; cdq; sub; sar`).

**Member states.**
- `Member_ResumeUnlessHeld800` - `+1` = 1 (back to `Member_Control`) unless
  bit 11 of `Field_ScriptFlags2` (`test ah, 8` of the dword `0x905BA4`):
  `Member_ResumeUnlessHeld` (state 3) with bit 10.
- `Member_FormActionState` - the member's form action. A stop when the
  leader (`ObjTrio +0x89`) is of kind 3 or 6 in its state 0xA step 1, when
  `Field_Request` is set, when the member is farther than a reach (each axis,
  `|d|` with the original's wrap, signed) from where the member it follows
  (`ObjTrio` record `+6`, unchecked) will be - that record's `+0x34` /
  `+0x38` plus its `+0xC` / `+0x10` times its `+9` - the reach 0x20000 when
  that record's `+0x70` byte is set, else 0x18000. Without a stop and with no
  bit of 0x1C00 in `Field_ScriptFlags2`, the handler of `Field_FormActions`
  for the party set is called; else `Field_State +0x137` = 0. Once
  `+0x137` is 0: the pose `+8`, `+9` = 0, `Member_ClearState(+5)`, `+1` = 1,
  `+0xB` = 0 and `Member_Follow` (a tail `jmp`). FE1's
  `Field_FormActionState` `0x52F4F0` is the leader's.
- `Member_JumpState` - `jmp Member_JumpSteps[+2]`: the leader's jump steps
  (`Field_JumpBegin`, `Field_JumpOut`, `Field_JumpIn`) with this group's step 2.
- `Member_JumpAir` - against the member it follows (`ObjTrio` record `+6`):
  that one's `+0x137` 3 - more than 0x80 apart in height (s16 words),
  `Leader_Sink` when above it, `Leader_Rise` when below; within 0x80
  nothing. Any other `+0x137`: `Leader_Rise` at or below its height,
  `Leader_TurnBack` above.

**The dispatchers** (25): each reads its index - `+2`, `+3` or the u16
`+0x2C` - and jumps through its table's word, the index unchecked (section
2). Ours calls the word in place (`CodeAt`): where it is not code the
original jumps into data and ours aborts with a message.

**The form actions' states** (shared by the sets' tables).
- `PartyFormAction_Form0Begin` - the pose `+4`: 0x41 without bit 0x16 of
  `Cond_Flags`' row 1 (`Flags_Test(0x903F98, 0x16)`); with it 0x50 when
  `Party_Count(0)` is 1 and `Field_InputFlags` lacks 0x40, else 0x40. By the
  pose: 0x40 facing 3; 0x41 facing 5; 0x50 facing 3 when `|+8 - 5| > |+8 -
  3|`, else facing 5 and the pose byte 0x52. Facing f: `+0xB` =
  `Sprite_TurnSense(f)`, `+3` = f. Then `+9` = 2, `+2` one on.
- `PartyFormAction_Form0Turn` - with `+8` come round to `+3`: the pose `+4`
  (`Sprite_SetAnimationAt(0x50, 2)` for 0x50, else `Sprite_SetAnimation`),
  `+2` one on; else `+9` counted down and at 0: `+9` = 2, `+8` = (`+8` +
  `+0xB`) & 7, the pose `+8`.
- `PartyFormAction_Form1Begin` - facing 7 when `|+8 - 1| > |+8 - 7|`, else 1;
  `+0xB` = `Sprite_TurnSense`, `+3` = the facing, `+9` = 2, `+2` one on.
- `PartyFormAction_Form1Turn` / `_Form2Turn` - as `_Form0Turn`'s turn, but on
  arriving the pose 0x40 for facing 1 and 0x41 for 7 (`_Form1Turn`), 0x41 for
  3 and 0x40 for 5 (`_Form2Turn`), none for any other.

**The actions' states.**
- `PartyAction{0,1,2}_Form0Begin`, `_Form0Resolve`, `_CellPickup` - set 5's
  (`docs/field_hidden.md`): the turn toward something two steps ahead
  (`PartyAction_TargetAhead`), the slope and side probes, the pose and a
  count of 5; then the object ahead marked and the map cell (and the cells one
  on across a fraction) picked up: 0xF2 a zenny find by `Rand`, 0xF8 item
  0x56 - each cleared after (`AreaMap_ClearCell`).
- `PartyAction0_Form2Begin` - with a kind-0x30 effect object lined up ahead
  (`PartyAction_Kind30Ahead`) and no member on or beyond it: its `+0xB` = 1,
  `Sound_PlayEffect(u16 +0x2C + 0x100)`, the pose `+8 + 8`, `Field_State
  +0x128` = 2, `Field_ScriptFlags` byte 3 |= 0x10, `Field_JumpStart`, `+9`
  one down, `Field_LeaderStepTick`, `Field_State +0x137` = 1, `+2` one on.
  With a member on or beyond it, `+0x137` = 0. With none: unless `Field_State
  +0x138` bit 0, the object two steps ahead (margin 1) is marked; `+0x137` = 0.
- `PartyAction2_Form2Aim` - an even `+8` turned one back; open ahead
  (`PartyAction_BlockedAhead` 0) - two on, and open again - back; then
  `PartyAction_SideProbes`, the pose (d - 1) / 2 + 0x42, `+0xB` = 0, `+0xA` =
  0xB, `+3` = 1. `PartyAction2_Form2Reaim` is the same without the turn and
  without `+0xB`.
- `PartyAction2_Form2Strike` - `+0xA` counted down; at 0:
  `Sound_PlayEffect(u16 +0x2C + 0x100)`; the effect object ahead
  (`Field_EffectAhead`) - its `+8` = the sprite's, its `+0xA` = 1, the
  sprite's `+6` = its index, `+3` one on (and again: two); none - the object
  two steps ahead marked (with sound 0x10B), then `PartyAction2_CellStrike` on
  the point's cell and the cells one on across a fraction while each answers
  0, `+3` one on. `Sprite_ScriptTickOnce` every time.
- `PartyAction2_CellStrike` `(x, z)` - by the map byte: 0xF0 / 0xF1 / 0xF4 an
  effect object of kind 0x34 on the cell (`Effect_SpawnAtCellHigh(0)`), a
  second (state 4) when `Rand & 7` is 6 or 7, sound 0x10B, al 1; 0xF6 / 0xF7
  the same first effect and sound, then by `Rand & 0xF`: below 7 an effect of
  state 3, item 0x29 (`Inventory_Add`; the message 2 or 3), `+0xB` = 2; above
  0xB an effect of state 2, `Sprite_FlashClut(0)`, `Char_LoseHp(1,
  Field_State +0x89)`, message 0xD9, `+0xB` = 1; then `Field_Request` = 2, al
  1; any other byte al 0.
- `PartyAction_FinishPalette` - `PartyAction_Finish` with the member's
  palette put back first: `+0xB` 1 - `Sprite_LoadPalette(0x80D380 + +5 *
  0x40, 0)` and `+0xB` = 2; then 0: `Sprite_ScriptTickOnce`, done when it
  answers; 2: `Sprite_ScriptTickOnce`, answering - `Sprite_SetAnimation(+8)`,
  `+0xB` = 3 -, done unless `Field_Request`; else `Sprite_ScriptTick`, done
  unless `Field_Request`. Done: `+0x2B` = 0, `Field_State +0x137` = 0.
- `PartyAction_WaitEffect` - the effect object `+0xB` names free (`+0`) or in
  its state 1, and `Field_Kind2Hold` 0: `+0x2B` = 0, `Field_State +0x137` =
  0. `Sprite_ScriptTick` every time.

## 2. The state tables

All are runs of code pointers in `.data`, back to back from `0x65F9A4` to
`0x65FAC8`; **no reader bounds its index** (no `cmp`), so each count is the
run up to the next table's start, read by hand. Named as `[[data]]` in
`symbols.toml` (22 new; `PartyAction{0,1,2}_Form0States`, `Member_States`,
`Field_FormActions` and `Field_ActionBySet` were named before):

| Table | At | Count | Read by | Entries |
|---|---|--:|---|---|
| `Member_JumpSteps` | `0x65F9A4` | 4 | `Member_JumpState` by `+2` | `Field_JumpBegin`, `Field_JumpOut`, `Member_JumpAir`, `Field_JumpIn` |
| `PartyFormAction0_Form0States` | `0x65F9B4` | 3 | `0x51BE90` by `+2` | `_Form0Begin`, `_Form0Turn`, `PartyAction_ScriptEnd` |
| `PartyFormAction0_Form1States` | `0x65F9CC` | 3 | `0x51C430` | `_Form1Begin`, `_Form1Turn`, `PartyAction_ScriptEnd` |
| `PartyAction0_Form1States` | `0x65F9D8` | 2 | `0x51C450` | `0x5252B0` (R1F), `0x521A20` (R1D) |
| `PartyFormAction0_Form2States` | `0x65F9E0` | 3 | `0x51C470` | `0x520840` (R1C), `_Form2Turn`, `PartyAction_ScriptEnd` |
| `PartyAction0_Form2States` | `0x65F9EC` | 2 | `0x51C510` | `PartyAction0_Form2Begin`, `0x521C40` (R1D) |
| `PartyFormAction0_Forms` | `0x65F9F4` | 3 | `0x51C740` by u16 `+0x2C` | `PartyFormAction0_Form0..2` |
| `PartyAction0_Forms` | `0x65FA00` | 3 | `0x51C760` by u16 `+0x2C` | `PartyAction0_Form0..2` |
| `PartyFormAction1_Form0States` | `0x65FA0C` | 3 | `0x51C780` | as set 0's |
| `PartyFormAction1_Form1States` | `0x65FA24` | 3 | `0x51CB80` | as set 0's |
| `PartyAction1_Form1States` | `0x65FA30` | 2 | `0x51CBA0` | as set 0's |
| `PartyFormAction1_Form2States` | `0x65FA38` | 3 | `0x51CBC0` | `0x520840`, `0x51FC80` (R1C), `PartyAction_ScriptEnd` |
| `PartyAction1_Form2States` | `0x65FA44` | 3 | `0x51CBE0` | `0x5239F0` (R1E), `0x51DE20` (R1B), `0x520350` (R1C) |
| `PartyFormAction1_Forms` | `0x65FA50` | 3 | `0x51CC00` by u16 `+0x2C` | `PartyFormAction1_Form0..2` |
| `PartyAction1_Forms` | `0x65FA5C` | 3 | `0x51CC20` by u16 `+0x2C` | `PartyAction1_Form0..2` |
| `PartyFormAction2_Form0States` | `0x65FA68` | 3 | `0x51CC40` | as set 0's |
| `PartyFormAction2_Form1States` | `0x65FA80` | 3 | `0x51D0D0` | as set 0's |
| `PartyAction2_Form1States` | `0x65FA8C` | 2 | `0x51D1E0` | as set 0's |
| `PartyFormAction2_Form2States` | `0x65FA94` | 3 | `0x51D200` | `0x520840`, `0x51FC80`, `BossOp_ScriptTick` |
| `PartyAction2_Form2States` | `0x65FAA0` | 2 | `0x51D220` | `_Form2State0`, `_Form2State1` |
| `PartyAction2_Form2State0Steps` | `0x65FAA8` | 5 | `0x51D240` by `+3` | `_Form2Aim`, `_Form2Strike`, `PartyAction_FinishPalette`, `0x51F850` (R1C), `0x522DE0` (R1E) |
| `PartyAction2_Form2State1Steps` | `0x65FABC` | 3 | `0x51D670` by `+3` | `_Form2Reaim`, `0x523F10` (R1F), `PartyAction_WaitEffect` |

`0x65FAC8` and `0x65FAD4` (set 2's forms tables) are read by R1B's
`0x51D710` / `0x51D730` and left for R1B to name. The cut's hint "table
`0x65FA50` read by `0x51CC00`" and the like are these.

## 3. Calling convention, arguments, answers

All 49 are `cdecl`. The 45 state handlers and dispatchers take nothing and
answer nothing any caller reads (their callers are `jmp`s through tables
and, at the top, `Field_MemberFrame` / `Field_ActionState` /
`Field_FormActionState`, which ignore eax). The four cell helpers take
`(x, z)` - a map cell each, which every callee they pass it to reads as 16
bits (`AreaMap_ByteAt`, `Effect_SpawnAtCell`, `Effect_SpawnAtCellHigh`
`movsx`; `AreaMap_ClearCell` `mov ax, word`) - and answer al 0 / 1, which
each of their three call sites tests first (`test al, al`); ours answers
`unsigned char`. Their callers push the point's high words as dwords whose
upper halves are their own uninitialised stack (as set 5's,
`docs/field_hidden.md` section 3); ours passes the word zero-extended, and
the fuzz compares 16 bits of each.

Every call out is a relative `E8` or the one tail `E9`; every jump stays
inside its function but the 25 dispatchers' `jmp [abs]`. Pushes of a whole
register for a narrower argument, and what the callee reads:
`Sprite_EnsureAnimation` / `Sprite_SetAnimation` (al over garbage: the low
byte), `Member_ClearState` (`and eax, 0xFF`), `Sound_PlayEffect` (a word
over garbage), `MapView_SlopeAt`'s direction (bl over the caller's ebx; the
standard field row reads a byte), R0A's `PartyAction_MemberOnEffect` /
`_MemberBeyondEffect` (the low byte), `Char_LoseHp`'s member (a byte).

## 4. The fuzz (`rest_1a_fuzz.cpp`)

`scenario_harness::Run`, field mode, **4,000 rounds per function**; the 45
handlers and dispatchers `Shape::kSprite`, the four cell helpers
`Shape::kCall` with `ret_mask 0xFF`. `BOF3X_R1A_ONLY=<name>` runs the clones
whose name contains it.

**The tables.** All 26 the group reads are `DataTable`s, swapped for
recorders while the fuzz runs (the 22 of section 2, the three named
`_Form0States`, `Field_FormActions` with its 19). A recorder is registered
per address, so the tables with the same entries (`0x65F9B4` / `0x65FA0C` /
`0x65FA68`; `0x65F9CC` / `0x65FA24` / `0x65FA80`; `0x65F9D8` / `0x65FA30` /
`0x65FA8C`) share theirs - a control swapping one for its twin is an
equivalent mutant (section 6). Each dispatcher's index is drawn below its
own table's count in the seed (`kDispatch`); `sprite_span` is not set.

**The callees.** The group's own stand-ins, registered before the harness's
standard rows: `Sprite_ObjectAt` (0xFF half the time, else 0..0x21 - what it
answers - with 0x1D / 0x1E / 0x21 often), `AreaMap_ByteAt` (each cell code
compared, 0xEF..0xF9, 0, 0xFF), `Effect_FindFree`, `Field_EffectAhead`,
`PartyAction_Kind30Ahead` (0xFF a third of the time, else 0..19),
`PartyAction_TargetAhead` / `_BlockedAhead` / `_MemberOnEffect` /
`_MemberBeyondEffect` (`kFlag`), `PartyAction_SideProbes` (writes `+0x2B`),
`Leader_Rise` / `_Sink` / `_TurnBack`, `Member_Follow`, `Effect_SpawnAtCell`
/ `_SpawnAtCellHigh` (byte, word, word), `AreaMap_ClearCell` (words),
`Rand` (its low nibble at every edge the code compares: & 0xF against 7,
0xB, 0xD, 0xF; & 7 against 5; & 3 against 0), `Field_GiveZenny`,
`MapView_SlopeAt` (`DamageScratch`'s flag 0 a third of the time; the slope
word either side of 0x40), `MapView_GroundAt` (around the sprite's height),
`Sprite_TurnSense` (0xFF / 1, any byte one in eight), `Member_ClearState`
(byte), `Sprite_FlashClut`, and the group's own four cell helpers (`kFlag`,
words) for the three resolves and the strike. The rest are the harness's
standard rows (`Sprite_EnsureAnimation`, `_SetAnimation`, `_SetAnimationAt`,
`Sound_PlayEffect`, `Sprite_ScriptTick` / `Once`, `Flags_Test`,
`Party_Count`, `Item_NamePtr` into the text buffer, `Inventory_Add`,
`Msg_OpenSystem`, `Field_JumpStart`, `Field_LeaderStepTick`, `Char_LoseHp`).
`Sprite_LoadPalette` is re-listed with its destination logged by value: the
field-standard row hashes 8 bytes at it (the callee only writes there), and
the palettes at `0x80D380` are alike at start-up, so a stride planted 0x20
for 0x40 passed it (C74). **For the harness's fold**: that row's `deref` of 8
on an out-parameter it never reads hides a wrong pointer wherever the bytes
there agree.

**The state.** Field mode's standard regions and the group's one:
`Text_Records`' first 16 bytes (the item paths copy a name in). 23,612 bytes,
39 regions.

**The seeds** (`Seed(k)`, after the harness's random fill):
- the member states run on an `ObjTrio` record (`Sprite_Current` set to one,
  as `Field_MemberFrame` runs them), `+6` 0..2;
- `Member_FormActionState`: the leader's kind 3 / 6 / other, its `+1` 0xA and
  `+2` 1 half the time each, `Field_Request` 0 two times in three, the
  followed record's `+9`, velocities and `+0x70`, the sprite's x and z at
  the target plus 0, 1, 0x17FFF..0x18001, 0x1FFFF..0x20001, their negatives
  or `0x80000000` (the wrap of `|d|`), `Field_ScriptFlags2`'s 0x1C00 clear
  two times in three, the party set below 19 (with bit 7 half the time);
- `Member_JumpAir`: `+0x137` 3 half the time, the two heights equal, 0x7F /
  0x80 / 0x81 apart either way, at the s16 limits or random;
- the turns: `+3` one of the facings compared, `+8` equal to it half the
  time, `+9` 1 mostly; `_Form0Turn`'s pose 0x50 half the time;
- the begins, the aim, the strike and the resolves: the direction 0..7 two
  times in three, else 8, 9, 15, 0x80, 0xFF or random (rows past the table,
  read in place); positions with fractions 0, 0x8000 ..; the point two steps
  ahead whole in x, in z or both half the time each (so the cells one on are
  probed or not); `+0xA` 1 mostly (0, 2, 0xFF too);
- `_Form0Begin` (form action): `Field_InputFlags` 0x40 flipped half the time;
  `PartyAction0_Form2Begin`: `Field_State +0x138` bit 0 clear half the time;
- `PartyAction_FinishPalette`: `+0xB` 0..3 or random, `Field_Request` 0 half
  the time; `PartyAction_WaitEffect`: `+0xB` 0..19, that record's `+0` and
  `+1` at 0 / 1 / 2, `Field_Kind2Hold` 0 half the time;
- the cell helpers' `(x, z)` (`Args`): the word 0, 1, 0x7FFF, 0x8000, 0xFFFF,
  0x40 or random under a random upper half; `Field_InputFlags` bits 1, 2
  clear half the time.

**The disturbance** (the group's case, from its hash only): the sprite's
direction, `+2` / `+3`, `+9..+0xB`, `+4` / `+5`, the form word, x or z,
height; `Field_State +0x137` / `+0x138`; `DamageScratch`'s flag;
`Field_InputFlags` bit 1 or 2; a dword of `Text_Records`. The harness's own
moves `Sprite_Current`, `Field_Request` and the rest of section 4 of its doc.

**Result** (2026-10-04, this worktree, `BOF3X_SELFTEST_ONLY=1
BOF3X_SHADOW=rest_1a`, exit 0, on the first run and again after C74's
re-listing, the same totals): 196,000 rounds over 49 functions,
312,245 calls to the stand-ins, **0 mismatches**; 333 stand-ins registered
(174 field-standard). Coverage: every handler of the 26 tables reached
(1,015 .. 10,545 calls each; `Field_FormActions`' 19 entries 18 .. 33 each,
the member's form action stopping before the call most rounds), every
callee called (`Sprite_FlashClut` / `Char_LoseHp` 172, `Field_GiveZenny`
305, `Field_JumpStart` 300, the cell strike 827, `Leader_Sink` 418 the
fewest). `BOF3X_SHADOW='*'` (2026-10-04, this worktree): exit 0, `inject:
8704 ours, 0 left original`, 1,020 self-test lines of 0 mismatches and none
other; the same with `BOF3X_WIDE=1`: exit 0, 1,020, 8,704 ours. Each passed
on its first run. `tools/ledger_check.py`: 0 errors (2 notes, not this
group's). The log lines "lies outside the field runs" name every clone: the
band `0x51BA80..0x51D70C` is in none of round twelve's field runs (a note,
not a refusal).

## 5. Latent defects and ranges (Capcom's, described, not fixed)

- **The dispatchers are unbounded** (all 25): a state, step or form beyond
  its table's run reads the next table's entries (every table here is
  followed by another of code pointers, to `0x65FAC8`), and past the run of
  tables the original jumps into data. Ours calls the word in place and aborts
  where it is not code (`CodeAt`). Whether play ever holds such an index is
  not established.
- **`Sprite_ObjectAt`'s answer indexes the object records unchecked**
  (`0x51C190`'s three copies and `0x51D2F0` compare it signed - `movsx`,
  `jge` -, `0x51C530` unsigned): an answer of 0x22..0xFE would mark a byte past
  `Sprite_ObjectsExtra`'s four records (or, signed, before `Sprite_Objects`).
  The callee answers 0..0x21 or 0xFF only (its `symbols.toml` evidence), so
  ours aborts on any other (`MarkObject`). Set 5's twin in `field_hidden.cpp`
  reproduces the write instead; the two agree on every answer the callee
  gives.
- **Effect indices from callees are used unchecked**: `0x51C530` writes
  record `PartyAction_Kind30Ahead`'s answer, `0x51D2F0` record
  `Field_EffectAhead`'s (as a signed byte); both answer 0..19 after the 0xFF
  test, so ours aborts past 19 (`EffectByAnswer`).
- **`PartyAction_WaitEffect` reads the record `+0xB` names, unchecked**
  (0..255 x 0x80 from `Effect_Objects`). Reproduced (a read in place, inside
  the exe's `.bss`), not aborted: what `+0xB` holds in this state is set by
  other groups' states (`0x523F10` reads it the same way), and that it stays
  below 20 is not established - the choice R0A made for the direction byte
  and `field_hidden` for `Member_EffectState`.
- **The member index `+6` is unchecked** (`0x51BAA0`, `0x51BD10`: `ObjTrio`
  + `+6` x 0x14C) - reproduced in place, as `member_sprites.cpp` reads it.
- **The direction is not masked** where a step is read (the begins, the
  resolves, `0x51C530`, `0x51D2F0`): reproduced, as R0A.
- **Ranges, not defects**: `|d|` wraps at `0x80000000` (stays negative, so
  never "beyond reach"); the cells one on wrap at 16 bits.

No full-frame fill, no float: nothing for DIV-0041.

## 6. Controls

`r1a/controls.py` (scratch): each plant replaces a string that occurs once in
`rest_1a.cpp`, rebuilds, runs the self-test on the clones it touches
(`BOF3X_R1A_ONLY`), restores the file and rebuilds. The count is the rounds
that mismatched (of 4,000 a clone).

**85 planted: 77 refused by a count; 3 equivalent mutants not refused, each
with its near variant refused; 4 refused by a fault only (an index past its
table into words the fuzz does not swap), each replaced by an in-table
variant that is refused; 1 not refused first (C74), the fuzz's fault, fixed
and refused (C74b).**

| # | Function | Plant | Refused |
|---|---|---|--:|
| C01 | ResumeUnlessHeld800 | bit 10 for bit 11 | 1,987 |
| C02 | FormActionState | leader kind 7 for 6 | 57 |
| C03 | FormActionState | the reach 0x18001 | 34 |
| C04 | FormActionState | the reach flag from `+0x71` | 148 |
| C05 | FormActionState | flags 0xC00 for 0x1C00 | 33 |
| C06 | FormActionState | `Member_ClearState(+6)` | 3,721 |
| C07 | FormActionState | z's velocity from `+0xC` | 225 |
| C08 | FormActionState | no `Member_Follow` tail | 3,734 |
| C09 | FormActionState | `+0xB` = 1 | 3,724 |
| C10 | FormActionState | `Field_State +0x136` tested | 3,714 |
| C11 | JumpAir | 0x80 apart counts (`>=`) | 176 |
| C12 | JumpAir | `mine >= theirs` for `>` | **not refused**: equivalent - in that branch the heights are more than 0x80 apart, never equal |
| C12b | JumpAir | above and below swapped (C12's near variant) | 704 |
| C13 | JumpAir | level turns back (`<` for `<=`) | 791 |
| C14 | JumpAir | `+0x136` for `+0x137` | 1,635 |
| C15 | JumpState | by `+3` | a fault only (`FieldCore_Recoil`'s Fatal: an index past the table reached real code) |
| C15b | JumpState | the step's neighbour, `+2 ^ 1` | 4,000 |
| C16 | FormAction0_ByForm | the action table `0x65FA00` | 4,000 |
| C17 | FormAction0_ByForm | the byte `+0x2C` for the word | **not refused**: equivalent in the domain - the seed draws the form below the table's 3, so the high byte is 0; a form above 0xFF indexes 256 entries past the table |
| C17b | FormAction0_ByForm | the word `+0x2E` | a fault only (random index past the table) |
| C17c | FormAction0_ByForm | the next form, `(+0x2C + 1) % 3` (C17's near variant) | 4,000 |
| C18 | Action0_Form1 | form 2's table | 4,000 |
| C19 | Action2_Form2State0 | by `+2` | a fault only (index past the table) |
| C19b | Action2_Form2State0 | the next step, `(+3 + 1) % 5` | 4,000 |
| C20 | FormAction1_Form0 | set 2's table `0x65FA68` | **not refused**: equivalent - the two tables hold the same three addresses (section 4) |
| C21 | FormAction1_Form0 | form 1's table (C20's near variant) | 2,623 |
| C22 | Action2_Form2State1 | the table one dword on | a fault only (its third word is outside the swapped tables) |
| C22b | Action2_Form2State1 | the next step, `(+3 + 1) % 3` | 4,000 |
| C23 | Form0Begin (x4) | back one, not two | 622 |
| C24 | Form0Begin | the steep pose 0x47 | 3,137 |
| C25 | Form0Begin | `+0xA` = 6 | 8,863 |
| C26 | Form0Begin | steep from 0x40 (`<`) | 852 |
| C27 | Form0Begin | `Sprite_Current` not re-read in the side probe | 179 (by the disturbance) |
| C28 | Form0Begin | the second probe pushes 3 | 8,863 |
| C29 | Form0Begin | odd directions turned | 12,000 |
| C30 | Resolve (x3) | the z probe one on in x too | 652 |
| C31 | Resolve | at 1, not 0 | 8,663 |
| C32 | Resolve | the found test inverted | 1,380 |
| C33 | (all 49) | `MarkObject`: 0x1E a sprite object | 155 |
| C34 | CellPickup (x3) | 5 zenny from 0xE | 67 |
| C35 | CellPickup | nine times | 30 |
| C36 | CellPickup | input bit 1 only | 47 |
| C37 | CellPickup | item 0x57's name | 1,338 |
| C38 | CellPickup | the clear's cell swapped | 2,424 |
| C39 | CellPickup | the second spawn state 2 | 316 |
| C40 | CellPickup | zenny from 0xC | 70 |
| C41 | Turns (x3) | the low facing posed high | 1,504 |
| C42 | Form1Turn | pose 0x42 | 739 |
| C43 | Form2Turn | facing 4 for 5 | 799 |
| C44 | Turns | `+9` = 3 | 1,889 |
| C45 | Form0Turn | `Sprite_SetAnimationAt` start 1 | 671 |
| C46 | Form0Turn | 0x52 tested | 1,070 |
| C47 | FormAction_Form0Begin | ties to facing 3 | 24 |
| C48 | FormAction_Form0Begin | pose 0x51 | 222 |
| C49 | FormAction_Form0Begin | input 0x20 | 317 |
| C50 | FormAction_Form0Begin | flag 0x17 | 4,000 |
| C51 | FormAction_Form1Begin | ties to facing 7 | 250 |
| C52 | Action0_Form2Begin | `+0x128` = 3 | 305 |
| C53 | Action0_Form2Begin | margin 0 | 1,004 |
| C54 | Action0_Form2Begin | `+0x138` bit 1 | 649 |
| C55 | Action0_Form2Begin | script flag 0x20 | 218 |
| C56 | Action0_Form2Begin | `+9` up | 304 |
| C57 | Action0_Form2Begin | the two member tests in the other order | 2,661 |
| C58 | Form2Aim | `+0xB` = 1 | 4,000 |
| C59 | Form2Aim | the second test inverted | 624 |
| C60 | Form2Reaim | pose 0x43 | 4,000 |
| C61 | Form2Strike | the effect's `+0xA` = 2 | 1,339 |
| C62 | Form2Strike | the index into `+7` | 1,339 |
| C63 | Form2Strike | `+3` once with an effect ahead | 1,326 |
| C64 | Form2Strike | the object's sound 0x10C | 311 |
| C65 | Form2Strike | the z probe whatever x's answered | 26 |
| C66 | CellStrike | the second spawn from 5 | 138 |
| C67 | CellStrike | the item below 8 | 36 |
| C68 | CellStrike | the hurt above 0xA | 29 |
| C69 | CellStrike | the member from `+0x88` | 178 |
| C70 | CellStrike | 0xF5 for 0xF4 | 462 |
| C71 | CellStrike | `+0xB` = 3 | 157 |
| C72 | CellStrike | item 0x28's name | 157 |
| C73 | CellStrike | `Field_Request` = 3 | 447 |
| C74 | FinishPalette | the palette stride 0x20 | **not refused**: the fuzz's fault - the standard `Sprite_LoadPalette` row hashes 8 bytes at `dst` (which the callee only writes) instead of logging it, and the palettes there are alike at start-up |
| C74b | FinishPalette | C74 again, `Sprite_LoadPalette` re-listed by value | 1,152 |
| C75 | FinishPalette | `+0xB` = 2 | 1,517 |
| C76 | FinishPalette | 3 tested for 2 | 2,849 |
| C77 | WaitEffect | state 2 | 609 |
| C78 | WaitEffect | the hold inverted | 2,812 |

The thinnest (C47 24, C65 26, C68 29, C35 30, C05 33, C03 34) each need a
narrow join: a direction exactly between two facings, an x and a z fraction
with the first probe answering 0, a `Rand` nibble of 0xB, the tenfold, the
script flags and the reach at their bit. The "(x4)" / "(x3)" rows ran every
clone the name matched (the three sets' copies and the form-action state of
the same name).

## 7. Calls across groups

**Out.** R0A's seven helpers by name (`game/rest_0a.h`; 18 sites:
`PartyAction_TargetAhead` six, `_BlockedAhead` two, `_SideProbes` two,
`_Kind30Ahead`, `_MemberOnEffect`, `_MemberBeyondEffect` one each,
`Effect_SpawnAtCellHigh` five). Every other callee is ours already (named)
or Capcom's `Rand` (`0x5B93D2`, by its name). **No raw call into another
group of the round** (`band_rows.py --edges`: R1A -> R0A only), so
`rest_1a_callees.h` holds data addresses, not callees. Through the tables
(no reference in code) the dispatchers reach other groups' states:
`0x5252B0`, `0x523F10` (R1F), `0x521A20`, `0x521C40` (R1D), `0x520840`,
`0x51FC80`, `0x520350`, `0x51F850` (R1C), `0x5239F0`, `0x522DE0` (R1E),
`0x51DE20` (R1B) - called by the word in place, whoever owns them.

**In (from outside the group, for the rebinding pass)** - all through
`.data`, none by `E8`: `Member_States` (`Field_MemberFrame`, ours) entries 4,
6, 8; `Field_ActionBySet` (`Field_ActionState`, ours) entries 0, 1;
`Field_FormActions` (FE1's `Field_FormActionState` and our
`Member_FormActionState`) entries 0, 1; R1B's `0x51D710` / `0x51D730`
through `0x65FAC8` / `0x65FAD4` (set 2's six form dispatchers); and the
shared states `0x51BEB0`, `0x51CC60`, `0x51C490`, `0x51D440`, `0x51D6D0` in
other sets' tables (`0x65FAE0` onward, read by R1B..R1F's dispatchers).
None needs a code change: a table's word is the original's address, which
the inject's `jmp` sends to ours.

## 8. The rebinding

`band_rows.py --refs --group R1A` and `grep -rn -i` for each of the 49 in
`src/game`: one reference, a comment - `member_sprites.cpp` line 153 names
`0x51BAA0` as "the end of state 6", describing `Member_Follow`'s entry from
its tail `jmp`. **Nothing to rebind**: no constant, call table or fuzz key
of ours names any of the 49, and no `scenario_harness*.cpp` or
`boss_harness*.cpp` row lists one (grep), so no harness row breaks. Left as
it is, on purpose: that comment (it is history and still true). The tables
are named in `symbols.toml` only; no source of ours indexes them but this
group's.

## 9. The live route

The catalog's reach column (`+`) marks the cut's first nine rows
(`0x51BA80..0x51C190`) - for hidden starts, by their **hosts'** reach
(`Field_DirectionTo`, `Member_Idle`, ours). No first-call or counted trace
under `analysis/calltrace` names any of the 49 (only the entry lists do), so
**no recorded route is known to enter them: fuzz only**. The coordinator's
frame hash and state hash cover whatever a route reaches after the merge.

## 10. For `analysis/calltrace/entries_logic.txt`

The main checkout's file had `0051C270 11F` (right) and three host lines
of this group's own starts whose extents ran on over the hidden starts after
them (`0051CA60 54B`, `0051CFB0 524`, `0051D4E0 550`). Appended: one line per
function not yet listed (45), each with the extent read here, and the
three hosts' own extents (0x11F, 0x11F, 0x188) as the brief's fix for a host
line. `0051C390 306` and `0051C6A0 3BB` (R0A's, covering this group's
`0x51C430..0x51C530` and `0x51C740..0x51C980`) are left for their owner.

## 11. Divergence

None. `DIVERGENCE.md`, `cheats.cpp` and `widescreen.cpp` name no byte in
`0x51BA80..0x51D70C` (grep, 2026-10-04). Ours aborts where the original
would jump into data or write past an object table on an answer its callee
never gives (section 5) - the project's rule, not a behaviour.
