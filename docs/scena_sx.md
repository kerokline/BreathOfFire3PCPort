# The engine callees nobody owned (group SX)

**Status:** IN PROGRESS (2026-09-28) - eighteen functions ours
(`src/game/scena_sx.cpp`, shadow name `scena_sx`), fuzzed headless through
the scenario harness ([`scenario_harness.md`](scenario_harness.md)):
**0 mismatches** in 54,000 rounds; **67 of 67 negative controls refused**
(66 by a count, one by a fault with its near variant refused by a count;
section 5). `BOF3X_SHADOW='*'` exit 0. One of the
nineteen addresses the round listed is not taken (`0x587B80`, section 1).
Fuzz only: no route is recorded through these.

Group SX of round ten's third wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) §3, §7, §9): the
engine functions the chapter groups (SC0..SC12, CALLS) and the area groups
(AR0B) called by raw address because nobody owned them. The model is group
SE ([`scena_se.md`](scena_se.md)).

## 1. The nineteen addresses, read

Each read to its last instruction (capstone; `tools/scenario_rows.py`'s
descent with the nineteen in place of SE's list gives the extents), and
each caller found by an `E8` / `E9` scan of `.text` and a dword scan of the
image (no dword anywhere holds any of the nineteen: none is in a table).

| Address | Bytes | Sites | What it is | Taken |
|---|--:|--:|---|---|
| `0x498DE0` | 0x1F3 | 2 | the level-up (the PSX `Char_LevelUp` `0x801AEDD4`) | `Char_LevelUp` |
| `0x532ED0` | 0xF9 | 58 | the members placed for an event battle | `Party_PlaceForBattle` |
| `0x533E00` | 0x48 | 63 | each member's palette reloaded | `Party_ReloadPalettes` |
| `0x533E50` | 0x94 | 51 | every joined record healed, the members' copies refreshed | `Party_HealJoined` |
| `0x534030` | 0x166 | 41 | a member out of the party | `Party_Remove` |
| `0x534DB0` | 0x96 | 13 | Sprite_Current's CLUT row one colour, sound `0x108` | `Sprite_FlashClut` |
| `0x537480` | 0x78 | 14 | a member's HP down, never below 1 | `Char_LoseHp` |
| `0x56D6F0` | 0x8 | 16 | `Field_StatusBits \|= 0x80` | `Field_SetStatus80` |
| `0x56D800` | 0xB0 | 8 | the chapters' 5-byte cell records searched | `Field_CellTriggerAt` |
| `0x56FCA0` | 0x77 | 9 | the view's 56 x 28 cell items refilled (PSX `0x80155154`) | `MapView_FillCells` |
| `0x57C550` | 0x47 | 3 | `Camera_Angles[0]` turned toward an angle in degrees | `Camera_TurnToDegrees` |
| `0x57C6B0` | 0xEE | 2 | `Cond_AngleFB` eased toward an angle over frames | `Camera_EaseAngleFB` |
| `0x57CD90` | 0x30 | 36 | the first free `Sprite_Objects` record | `Sprite_FindFree` |
| `0x587B80` | 0x5 | 21 | `jmp 0x5A6FF0`: the sound module's music stop | **no** (named `Sound_StopMusic`) |
| `0x590C90` | 0x4F | 13 | an ability into a list (PSX `AbilityList_Add`) | `AbilityList_Add` |
| `0x591900` | 0x20 | 12 | a key item into the 32-byte list | `KeyItem_Add` |
| `0x591B60` | 0x5D | 29 | items out of a category (PSX `Inventory_Remove`) | `Inventory_Remove` |
| `0x591BC0` | 0x1C | 4 | the zenny down if enough (PSX `Zenny_Sub`) | `Zenny_Sub` |
| `0x591BE0` | 0x3A | 15 | the zenny up, capped (PSX `Zenny_Add`) | `Zenny_Add` |

The four sibling names (`Char_LevelUp`, `AbilityList_Add`,
`Inventory_Remove`, `Zenny_Sub` / `Zenny_Add`) come from the sibling's
`symbols.toml` / `names/functions.toml` through `pairs_propagated.json`;
each was taken only because the PC reading says the same thing.

**`0x587B80` is left to the sound module.** It is the five bytes `jmp
0x5A6FF0`, and `0x5A6FF0` is byte for byte `Music_Stop` `0x5A7050` (ours,
`sound.cpp`): `Music_Buffer`'s `GetStatus` into a local, `Stop` when
playing - where the local is the slot of its `push ecx`, so when
`GetStatus` fails without writing, the test reads the caller's `ecx`
(`Music_IsPlaying` is taken through a naked entry for exactly this).
`Sound_PauseAll` tail-jumps to `0x5A6FF0` as well, and `Sound_ResumeAll`
`0x587B90` is the same kind of thunk (to `0x5A7080`, unread). A C
replacement of the thunk would call `0x5A6FF0` with ours's `ecx`, not the
caller's; a naked `jmp` replacement changes nothing. It belongs with the
thunk layer `0x587B80` / `0x587B90` and `0x5A6FF0` / `0x5A7080` in the sound
module's round. It is **named** (`Sound_StopMusic`, no `impl`) so the
rebinding pass can call it by name; its 21 sites are in section 3.

**`0x532FD0`, `0x591EC0` and `0x57C5A0` stay raw.** Each is the one callee
of one of ours (`Party_PlaceForBattle`, `AbilityList_Add`,
`Camera_TurnToDegrees`), nobody's and not in the round's list:
`0x532FD0` places `Sprite_Current` at the formation's offset for a slot
(`0x660B1C` byte pairs, `0x6698B0` rows, `MapView_GroundAt`); `0x591EC0` is
`Char_AbilityList`'s twin answering one of a record's (or its `ObjTrio`
copy's) four 10-byte lists by the id's class byte `0x65C4D9[id * 24] & 3`;
`0x57C5A0` turns `Camera_Angles[0]` by a step toward an angle and sets
`Light_AnglesCopy` at the end (with `0x57C600` / `0x57C650`, its twins for
`Cond_AngleFB`). Candidates for whoever takes the camera and the ability
lists.

## 2. The functions

| Function | Address | Args | Answers |
|---|---|---|---|
| `Char_LevelUp` | `0x498DE0` | the record (a byte) | nothing read |
| `Party_PlaceForBattle` | `0x532ED0` | x, z (16.16), the event battle (a byte) | nothing read |
| `Party_ReloadPalettes` | `0x533E00` | none | nothing read |
| `Party_HealJoined` | `0x533E50` | none | nothing read |
| `Party_Remove` | `0x534030` | the member id (a byte) | nothing read |
| `Sprite_FlashClut` | `0x534DB0` | the colour (a byte, 0..3) | nothing read |
| `Char_LoseHp` | `0x537480` | the amount (its word compared), the member id | eax: what it took (`Field_Bit80Tick` reads `ax`) |
| `Field_SetStatus80` | `0x56D6F0` | none | nothing |
| `Field_CellTriggerAt` | `0x56D800` | the table, a count, x, z (bytes) | eax: the record's index, or 0xFF |
| `MapView_FillCells` | `0x56FCA0` | none | nothing read |
| `Camera_TurnToDegrees` | `0x57C550` | the angle (s16), the step (s8), degrees | `0x57C5A0`'s eax (al 1 while turning) |
| `Camera_EaseAngleFB` | `0x57C6B0` | the angle (s16 degrees), a frame multiplier (s8) | al 1 while easing |
| `Sprite_FindFree` | `0x57CD90` | none | al: 0..29, or 0xFF |
| `AbilityList_Add` | `0x590C90` | id, member, shared, which | al 1 placed, 0 not |
| `KeyItem_Add` | `0x591900` | the item | al 1 placed, 0 full |
| `Inventory_Remove` | `0x591B60` | category, item, count | al 1 taken, 0 not |
| `Zenny_Sub` | `0x591BC0` | the amount (u32) | al 1 paid, 0 short |
| `Zenny_Add` | `0x591BE0` | the amount, a flag (byte) | al 1, or 0 at the cap |

What each does (the `evidence` fields have the addresses):

- **`Char_LevelUp(n)`**: record n's EXP (`+0xC`, compared signed) against
  the running sum of its 99 `Char_ExpTable` rows' words gives the new level
  (99 at most). At 99 already, or not higher, nothing. Else for each level
  gained, from the old level's row: the six stat words `+0x40..+0x4A` grow
  by row bytes `+2`, `+3` and the nibbles of `+4`, `+5` (high first), each
  plus the record's signed bias byte `+0x89..+0x8E`, never below 0, the
  16-bit word capped at 999; then `AbilityList_Add(row +6, n, 0, 0)` and
  `(row +7, ...)`. Then the level `+0xA` and `Char_RecalcStats`. The
  `symbols.toml` hypothesis on `Char_ExpTable` ("row bytes +2..+7 are the
  level-up gains") is now read: +2..+5 the stats, +6 / +7 two abilities.
- **`Party_PlaceForBattle(x, z, n)`**: x, z to `0x903780` / `0x903784`,
  the formation `0x904AAC` from `EventBattle_Records[n] +1`; each member i:
  `Field_State` and `Sprite_Current` its `ObjTrio` record, its slot j (the
  index of `+0x89` in the second party list, 3 if absent), `0x532FD0(x, z,
  j)`, the placed `+0x34` / `+0x38` saved at `0x7E06E0 + 8 j`, `+0x48` 0,
  the animation `+8` from `BattleFormation_Anims[formation]` and
  `Sprite_EnsureAnimation`. The slot is written into the low byte of the
  third argument's own stack slot and passed as that dword: its upper bytes
  are the caller's (ours carries them).
- **`Party_ReloadPalettes`**: per member, `Sprite_Current` its record,
  `Sprite_ReleaseTint`, `Sprite_LoadPalette(0x80D380 + 0x40 i, 0)`.
- **`Party_HealJoined`**: each of the eight records with `+0xB` bit 0:
  `+0x1E` 0, `Char_RecalcStats`, then `+0x1C` = `+0x2E`, HP = max HP
  (`+0x18` = `+0x20`), AP = max AP (`+0x1A` = `+0x22`), the state word
  `+0x10` 0; then each member's record copied over its `ObjTrio` record
  `+0x80`. The inn's `Party_RestoreAll` `0x580630` does more
  (`Char_ClearStatus`, `+0x1D`, a flag test).
- **`Party_Remove(id)`**: `Party_JoinReset`; the id's slot in each list
  becomes 0xFF and the lists close up (the first list's close-up moves each
  following `ObjTrio` record down and renumbers it: `+5`, `+0x26`, `+0x27`,
  `+0x4B`, `+0x148`); the last record's `+0` cleared and
  `Member_ClearState(count - 1)`; `0x904060` cleared at two members, or at
  three when it is 3 or more; the count one less.
- **`Sprite_FlashClut(c)`**: words 1..31 of `Sprite_Current`'s CLUT row
  (`+0x27`) in `Gfx_ClutStrip` one of four colours, the strip dirty, sound
  `0x108`, `Field_State +0x138 |= 8`. The field actions call it with 0,
  `Field_Bit80Tick` with 1, `0x505480` with 2.
- **`Char_LoseHp(amount, id)`**: HP down by the amount's word while HP is
  more, else HP 1; then the record `Field_State +0x148` names (not
  necessarily the member's) gets bit 0x2000 in its state word, and
  `Field_State +0x90` too, when its HP is at most a quarter of its max.
- **`Field_SetStatus80`**: one `or`.
- **`Field_CellTriggerAt(table, count, x, z)`**: the first of `count`
  5-byte records (area, x, z, flags, run length) in `Game_AreaNumber` whose
  run holds the cell (flags bit 7: along z, else along x) and whose facing
  (flags low nibble) is 8 or the leader's `+8`. The chapters' cell hooks
  (`Scena05_CellRecords` and the like) call it.
- **`MapView_FillCells`**: `MapView_CellToMap(i, j, &MapView_CellItems[row
  * 28 + column])` over 56 x 28, starting one past `MapView_Row` /
  `MapView_Column` and wrapping, then `MapView_PlaceRuns`.
- **`Camera_TurnToDegrees(a, s)`**: degrees to the 4096-step circle
  (`x * 4096 / 360`, the `0xB60B60B7` idiom) and `0x57C5A0`.
- **`Camera_EaseAngleFB(a, f)`**: a small state machine in four dwords:
  while frames are left, a 16.16 step a frame into `Cond_AngleFB`; else
  either the angle at once (a kind-2 sprite whose `Field_MoveSpeeds` entry
  is 0) or a new ease of `0x80 / (speed x 4 or 8) x f` frames.
- **`Sprite_FindFree`**, **`AbilityList_Add`**, **`KeyItem_Add`**,
  **`Inventory_Remove`**, **`Zenny_Sub`**, **`Zenny_Add`**: as the table and
  `symbols.toml` say. `Zenny_Add` also adds into `0x904138` when its flag
  byte is 0 (a tally; what reads it is not read here).

`symbols.toml` gains two `[[data]]`: `Party_Zenny` `0x904058` and
`BattleFormation_Anims` `0x660B3C`.

## 3. Who calls each (for the rebinding pass)

By `E8` / `E9` site, grouped by band (the round's bands; area sites by
`analysis/area_funcs.tsv`'s unit where no group has the area yet;
"engine" by the containing function's owner). Every merged group calls
these through a raw address in its `_callees.h` (or `SH_AT`, or the
harness's standard set by address); after this merge each is ours at that
address, so the raw calls stay correct until the rebinding pass names them.

| Function | Sites by band |
|---|---|
| `Char_LevelUp` `0x498DE0` | engine: `BattleResult_Setup` (`battle_result.cpp` `kLevelUp`) 1 (`0x431DE9`); SC7 1 (`0x553641`, `scena_sc7_callees.h` `kCall498DE0`) |
| `Party_PlaceForBattle` `0x532ED0` | SC0 5 (`0x538EF5`, `0x539004`, `0x539081`, `0x5390E2`, `0x539203`); SC1 2 (`0x53B4B9`, `0x53BB81`); SC2 8 (`0x53EDB3`, `0x53F41E`, `0x53F76B`, `0x53F849`, `0x53FA6B`, `0x5402B5`, `0x54084E`, `0x541046`); SC3 1 (`0x5431FE`); SC5 12 (`0x546A44`, `0x546C06`, `0x547F2C`, `0x5482CB`, `0x548420`, `0x54877A`, `0x5487A2`, `0x5487C9`, `0x548E5F`, `0x548E86`, `0x548EAD`, `0x549CB0`); SC6 6 (`0x54AC80`, `0x54C5A1`, `0x54C664`, `0x54CF8C`, `0x54D0D6`, `0x54D8C1`); SC7 6 (`0x54F901`, `0x54FC15`, `0x550287`, `0x551AFD`, `0x55231F`, `0x55330D`); SC9a 3 (`0x55585E`, `0x555A46`, `0x555C42`); SC9b 2 (`0x559A01`, `0x55B3E5`); SC12 2 (`0x561579`, `0x5615E2`); SC13 9 (`0x563DEA`, `0x56476E`, `0x5647C4`, `0x5649BD`, `0x5652EA`, `0x565494`, `0x5661C9`, `0x567331`, `0x567518`); SC15 2 (`0x5685AF`, `0x56A15F`); the harness's standard set by address |
| `Party_ReloadPalettes` `0x533E00` | CALLS 63 (53 tail jmps; `scena_calls_callees.h`); SE's `Scena08_PartyJoin784` (`scena_se_callees.h` `kPartyPalettes`) |
| `Party_HealJoined` `0x533E50` | SC1 7 (`0x53B436`, `0x53C54A`, `0x53CBF2`, `0x53CD47`, `0x53CE71`, `0x53D174`, `0x53D2B9`); SC2 7 (`0x53E762`, `0x53F53D`, `0x53FC74`, `0x53FDB4`, `0x54045B`, `0x540927`, `0x541283`); SC3 4 (`0x54315F`, `0x54337B`, `0x543CA8`, `0x54564A`); SC5 2 (`0x547132`, `0x549DCC`); SC6 10 (`0x54B643`, `0x54BA32`, `0x54C274`, `0x54C4A9`, `0x54C5D1`, `0x54C757`, `0x54C827`, `0x54CA80`, `0x54CB36`, `0x54CBCD`); SC7 4 (`0x54FA54`, `0x55107D`, `0x55204D`, `0x5524B7`); SC9a 3 (`0x5553ED`, `0x555E8C`, `0x556928`); SC12 3 (`0x55EE51`, `0x5606BA`, `0x5609BD`); SC13 3 (`0x561DCC`, `0x56265C`, `0x565AF9`); SC17 block 1 (`0x56D40C`); areas 141, 149, 169, 171, 192 (`0x4203F5`, `0x423871`, `0x426A9D`, `0x427978`, `0x42C660`); engine: `Field_ModeTailRun` 2 (`0x56DBCD`, `0x56DD04`) |
| `Party_Remove` `0x534030` | CALLS 41 (`scena_calls_callees.h`) |
| `Sprite_FlashClut` `0x534DB0` | SC11 1 (`0x55CA7C`); engine: `Field_FloorDamage` (`0x534C12`), `Field_Bit80Tick` (`0x534FAA`) - both `event_objs_callees.h`; nobody's: `0x505480` (`0x505723`) and the nine field-action copies `0x51D4E0`, `0x51E6C0`, `0x51EF30`, `0x51F880`, `0x520C80`, `0x522320`, `0x522E20`, `0x523D20`, `0x524BB0` |
| `Char_LoseHp` `0x537480` | SC11 2 (`0x55CA85`, `0x55D4FC`); engine: `Field_Bit80Tick` 2 (`0x534FF3`, `0x535039`); nobody's: `0x534C20` (`0x534CBC`) and the same nine field-action copies |
| `Field_SetStatus80` `0x56D6F0` | SC0 1 (`0x539921`); SC1 1 (`0x53CB7F`, a tail jmp); SC2 1 (`0x5409FF`); SC3 2 (`0x5447C3` a tail jmp, `0x54607F`); SC5 1 (`0x5463E4`); SC6 1 (`0x54ABC8`); SC7 2 (`0x550797`, `0x55354C`); SC9a 1 (`0x557027`); SC9b 1 (`0x55AE99`); SC11 1 (`0x55E114`); SC12 1 (`0x5617C0`); SC13 2 (`0x5640E9`, `0x5676FA`); SC15 1 (`0x56AC82`); the harness's standard set by address |
| `Field_CellTriggerAt` `0x56D800` | SC1 1 (`0x53DD31`); SC3 2 (`0x544FC1`, `0x546351`); SC5 1 (`0x54A811`); SC6 1 (`0x54EEE1`); SC7 1 (`0x550FE1`); SC9b 1 (`0x557A31`); SC12 1 (`0x561CF1`) |
| `MapView_FillCells` `0x56FCA0` | SC0 2 (`0x53880C` a tail jmp, `0x53964C`); SC13 1 (`0x566058`); SC16 1 (`0x56BC84`, a tail jmp; `field_modes_callees.h` `kViewShift`); areas 149 3 (`0x4233E1`, `0x4234C7`, `0x42358E`), 185 2 (`0x42A06C`, `0x42A104`); the harness's standard set by address |
| `Camera_TurnToDegrees` `0x57C550` | SC1 3 (`0x53B221`, `0x53B2C1`, `0x53C458`) |
| `Camera_EaseAngleFB` `0x57C6B0` | SC0 2 (`0x5394CB`, `0x539560`); the harness's standard set by address |
| `Sprite_FindFree` `0x57CD90` | SC3 1 (`0x544833`); SC7 3 (`0x5508D2`, `0x552CA0`, `0x552CC8`); SC9b 1 (`0x5594D0`); SC13 4 (`0x562705`, `0x562729`, `0x5627A5`, `0x5627C9`); SC15 2 (`0x56973C`, `0x56976A`); AR1A 1 (`0x405770`); AR1B 1 (`0x406EE0`); areas 65, 131, 141 (8), 191, 192, 198; nobody's engine `0x46A1E0` 4, `0x477D20` 2, `0x47B7D0` 2, `0x487BF0` 2 |
| `Sound_StopMusic` `0x587B80` (not taken) | SC2 1 (`0x54128F`); SC3 3 (`0x543288`, `0x5433AA`, `0x544B05`); SC6 1 (`0x54CCCA`); SC7 4 (`0x54FA59`, `0x55047F`, `0x551BD6`, `0x5524BC`); SC9b 1 (`0x55938C`); SC11 1 (`0x55D5F1`); SC13 2 (`0x56636F`, `0x567692`); areas 52 (`0x40B2D0`, a tail jmp), 112, 135, 190, 191; engine: `Field_ModeTailRun` 3 (`0x56DBD2`, `0x56DD09`, `0x56DEF7`) |
| `AbilityList_Add` `0x590C90` | `Char_LevelUp` 2 (ours, by name); SC2 1 (`0x5420AA`); SC3 1 (`0x542DDB`); engine: `Battle_SettleFlag8` 2 (`battle_sprites_callees.h`); nobody's `0x43C9F0` 4, `0x584120`, `0x586D20`, `0x58D7B0` |
| `KeyItem_Add` `0x591900` | SC3 1 (`0x544AF2`); SC6 1 (`0x54B5D2`); SC7 1 (`0x550BBF`); SC9b 1 (`0x559387`); SC13 2 (`0x566381`, `0x5676A0`); AR1A 1 (`0x405736`); areas 91, 113, 121, 135, 143 |
| `Inventory_Remove` `0x591B60` | SC2 1 (`0x540D8D`); AR0B 1 (`0x402F42`); AR1A 4 (`0x406341`, `0x40634E`, `0x40635B`, `0x406368`); areas 52, 76, 88; engine: `Shop_Equip` 6 (`save_menu_callees.h`); nobody's `0x42E250`, `0x449A00`, `0x45FCE0`, `0x47F2D0`, `0x5298A0` 3, `0x57DFF0` 4, `0x584120`, `0x58D570`, `0x594D90` |
| `Zenny_Sub` `0x591BC0` | SC2 3 (`0x540B1D`, `0x540C0E`, `0x540C8C`); AR0B 1 (`0x402EA0`, `Area21_ChoicePay`) |
| `Zenny_Add` `0x591BE0` | SC2 1 (`0x540A3D`); SC6 2 (`0x54BE02`, `0x54C0CA`); SC7 1 (`0x551ED0`); SC15's `0x537580` 6; AR0B 1 (`0x402F35`); engine: `BattleResult_ZennyTick` 2 (`battle_result_callees.h` `kAddZenny`); nobody's `0x459EE0`, `0x5307C0`; `event_leader_callees.h` and `field_hidden_callees.h` list it |

The harness's standard set (`scenario_harness.cpp`, `kStandard`) lists
`0x532ED0`, `0x57C6B0`, `0x56FCA0` and `0x56D6F0` **by address**, not
`SH_THEIRS`: that listing keeps working now they are ours (a by-address
key is Capcom's code range, and the merged groups' `SH_AT` calls look them
up by that key), so **the harness is not edited**. Moving them to
`SH_OURS` belongs to the rebinding pass, together with every group's
`SH_AT` of them - moving one without the other would leave the groups'
raw calls without a stand-in.

## 4. The fuzz (`scena_sx_fuzz.cpp`)

Through the scenario harness, all eighteen clones called with ten words,
3,000 rounds each, `g.chapter = 0` (none reads the chapter bytes or the
flag row). Clone table: `scenario_rows.py`'s machinery with the nineteen
in place of SE's list (a scratch driver), names given, `0x587B80` out;
**one correction**: the tool's descent stops `MapView_FillCells` at 0x10
bytes (`REFUSED +0xE short jmp out to 0x56fcb4`) because its entry jumps
over its own loop head; it runs to `+0x76`, and its two calls
(`MapView_CellToMap` `+0x58`, `MapView_PlaceRuns` `+0x6C`) are added by
hand. A tool gap for `scenario_rows.py` / `magic_rows.descend`: a forward
`jmp` inside the function taken for a tail jump.

- **Callees** the group lists (beyond the standard set's
  `Sprite_EnsureAnimation`, logged whole): ours by name -
  `AbilityList_Add` (for `Char_LevelUp`), `Char_RecalcStats`,
  `Sprite_ReleaseTint`, `Sprite_LoadPalette`, `Party_JoinReset`,
  `Member_ClearState`, `MapView_CellToMap`, `MapView_PlaceRuns`,
  `Sound_PlayEffect`; nobody's by address - `0x532FD0` (its third word
  whole, so the caller's upper bytes are compared), `0x591EC0` (a custom
  answer: a pointer to one of a record's four 10-byte lists inside the
  region, as the real one answers), `0x57C5A0`. Every argument logged
  whole except `AbilityList_Add`'s four bytes and `Sound_PlayEffect`'s
  word.
- **Stir**: each of the group's callees except `0x57C5A0` and `0x591EC0`
  moves one cell after it has logged (from the recorders' stream): a
  record's level, bias, stat, HP / AP or flag byte; the member count
  (0..3); the formation byte; `Field_State`; `MapView_Column`; `0x904060`;
  a party-list byte; the event battle's x or z; `Sprite_Current`; an
  `ObjTrio` record's `+0x89`. The group's `disturb` moves the same set from
  the hash it is given.
- **Regions** beyond the harness's 22: `CharacterRecords` 0..7, the
  inventory block `0x904098..0x9045F4` (the tally `0x904138`, the four
  categories' lists, the key items, the shared ability list), the battle
  bytes, `0x903780` (8), `0x7E06E0` (0x20), `Gfx_ClutStrip` (0x4000, all
  256 rows), the ease's `0x905B78` and `0x905B98` (8), the 0x14C bytes
  before `ObjTrio` (`Party_Remove` clears `+0` there at a count of 0), and
  a 40-byte cell-record table of the fuzz's own: 32 regions, 29,664 bytes.
- **Seeds**: the member count 0..3 (mostly 1..3); both lists' ids below
  24, the second often the first's in another order, a 0xFF now and then;
  `ObjTrio` `+0x89` from the list, `+0x148` 0..7. Per function:
  `Char_LevelUp` - old levels 0, 1, 97, 98, 99, above 99 and in between,
  the EXP at, one below or above a target level's sum (0..5 levels on),
  sometimes negative or `0x7FFFFFFF`, stat words at 998, 999, 1000 and
  `0xFFF0..`; `Party_HealJoined` - the joined bit both ways;
  `Char_LoseHp` - HP 0, 1, 2, a quarter of max and one above, max;
  `Field_CellTriggerAt` - `Game_AreaNumber` a byte two times in three,
  records in its area, cells 0..7, runs 0..4 and 0xFF, facings 8, the
  leader's or other, the arguments on a record's run or near it;
  `MapView_FillCells` - row and column 0, the last-but-one, the wrap
  value, in range, any s16; `Camera_EaseAngleFB` - frames left 0, 1..3,
  -1, up to 40; the speed index 0..5 (speeds 0, 1, 2, 4, 8, 16); the mode
  2, 6 or other; an angle in -720..720 and a multiplier of +-1..5 (never
  0: the original divides by it); `Sprite_FindFree` - every record in use,
  one or two free; the lists (`AbilityList_Add`, `KeyItem_Add`) full, or
  with a hole; `Inventory_Remove` - categories 0..4, the item planted in a
  slot, the count at, above, below or half the stack, item or count 0 now
  and then; zenny 0, near the amount, near and at the cap, near 2^32.

**Result** (2026-09-28, in this worktree, `BOF3X_SELFTEST_ONLY=1
BOF3X_SHADOW=scena_sx`, exit 0): 54,000 rounds over 18 functions,
4,771,325 calls to the stand-ins, **0 mismatches**, 29,664 bytes (32
regions). Coverage: `AbilityList_Add` 16,074 (the level-ups' abilities),
`Char_RecalcStats` 14,087, `Sprite_ReleaseTint` / `Sprite_LoadPalette`
5,191, `Party_JoinReset` 3,000, `Member_ClearState` 3,000,
`MapView_CellToMap` 4,704,000, `MapView_PlaceRuns` 3,000,
`Sound_PlayEffect` 3,000, `0x532FD0` 5,385, `Sprite_EnsureAnimation`
5,385, `0x591EC0` 1,012, `0x57C5A0` 3,000.

Under `BOF3X_SHADOW='*'` (every group of every harness): 54,000 rounds,
0 mismatches, exit 0 (the call count moves with the other groups' state,
as round nine recorded).

## 5. Controls

Planted one at a time in `scena_sx.cpp` by a script (plant, rebuild, run,
restore; rebuilt at the end), each anchored on a unique string. The count
is mismatching rounds of 3,000 for the function (the function's first
differing round in brackets); every refused one exited 3 but C61.

| # | Function | Mutant | Mismatches |
|---|---|---|--:|
| C1 | `Char_LevelUp` | sum >= exp | 798 (0) |
| C2 |  | stat +0x44 from the low nibble | 282 (16) |
| C3 |  | cap 998 | 2027 (0) |
| C4 |  | row +6 twice | 934 (0) |
| C5 |  | stop at 98 | 279 (7) |
| C6 |  | a negative gain made positive | 1594 (0) |
| C7 | `Party_PlaceForBattle` | the record +2 | 2454 (0) |
| C8 |  | the search stops at 2 | 1976 (1) |
| C9 |  | z from +0x34 | 2756 (1) |
| C10 |  | the animation dword zero-extended | 2756 (1) |
| C11 |  | x passed, not re-read | 134 (3) |
| C12 | `Party_ReloadPalettes` | palettes 0x20 apart | 1718 (3) |
| C13 |  | Sprite_Current not set | 2001 (0) |
| C14 |  | the count read once | 508 (5) |
| C15 | `Party_HealJoined` | bit 1 for bit 0 | 2987 (0) |
| C16 |  | +0x2E read before the call | 9 (272) |
| C17 |  | AP from +0x20 | 2985 (0) |
| C18 |  | the second list | 1598 (1) |
| C19 |  | the state word kept | 2985 (0) |
| C20 | `Party_Remove` | the second list searched one short | 1678 (0) |
| C21 |  | +0x27 0x77 + slot | 734 (1) |
| C22 |  | 0x904060 cleared at 4 or more | 192 (22) |
| C23 |  | Member_ClearState(count) | 3000 (0) |
| C24 |  | the count not re-read after the call | 236 (45) |
| C25 |  | +0x148 the id itself | 572 (1) |
| C26 | `Sprite_FlashClut` | colour 2 0x0201 | 791 (6) |
| C27 |  | word 0 too | 3000 (0) |
| C28 |  | the dirty flag not set | 2990 (0) |
| C29 |  | +0x138 \|= 4 | 2237 (0) |
| C30 | `Char_LoseHp` | HP equal to the amount goes to 0 | 183 (50) |
| C31 |  | a half, not a quarter | 300 (16) |
| C32 |  | the member tested, not Field_State +0x148 | 1828 (1) |
| C33 |  | Field_State +0x90 \|= 0x1000 | 1389 (0) |
| C34 | `Field_SetStatus80` | \| 0x40 | 2216 (0) |
| C35 | `Field_CellTriggerAt` | facing 8 only | 74 (54) |
| C36 |  | the z run one on | 53 (33) |
| C37 |  | the run stops before k 0 | 50 (63) |
| C38 |  | the x run tests z against +1 | 84 (13) |
| C39 |  | the area word as a byte | 81 (19) |
| C40 | `MapView_FillCells` | rows wrap at 0x36 | 2514 (1) |
| C41 |  | the column read once | 3000 (0) |
| C42 |  | row and column indexes swapped | 3000 (0) |
| C43 | `Camera_TurnToDegrees` | the angle & 0x7FF | 1495 (1) |
| C44 |  | the step unsigned | 1465 (0) |
| C45 | `Camera_EaseAngleFB` | al 1 on the last frame | 319 (1) |
| C46 |  | mode 5 for 6 | 387 (5) |
| C47 |  | the step from angle & 0xFFF | 611 (5) |
| C48 |  | the accumulator from the masked angle | 1199 (3) |
| C49 |  | the high word >> 15 | 1520 (0) |
| C50 |  | speed 0 sets the angle masked | 179 (4) |
| C51 | `Sprite_FindFree` | the last record never | 43 (34) |
| C52 |  | none answers 0xFE | 490 (0) |
| C53 | `AbilityList_Add` | nine member slots | 11 (144) |
| C54 |  | 127 shared slots | 5 (140) |
| C55 |  | the list by `shared`, not `which` | 1012 (4) |
| C56 |  | `shared` as a word | 1006 (4) |
| C57 | `KeyItem_Add` | 31 slots | 70 (90) |
| C58 |  | the item \| 1 | 965 (0) |
| C59 | `Inventory_Remove` | all of a stack refused | 443 (0) |
| C60 |  | the id kept at a count of 1 | 200 (18) |
| C61 |  | category 3 as the key items | fault (0xC0000005: category 4's null count list read), exit 0xC0000005 |
| C61b |  | category 3 skips the counts | 391 (4) |
| C62 |  | 127 slots | 6 (918) |
| C63 | `Zenny_Sub` | the exact amount refused | 457 (3) |
| C64 | `Zenny_Add` | the cap itself refused | 430 (2) |
| C65 |  | the tally on flag not 0 | 2523 (0) |
| C66 |  | not capped | 1732 (0) |

**67 planted, 67 refused**: 66 by a count, C61 by a fault. C61 (category
3 taken for the key items) sends category 4 down the count path, which
reads its null count-list pointer - an access violation in the self-test,
not a count; its near variant C61b (category 3 skips the count path) is
refused by a count. No equivalent mutant. The low counts (C16, C53, C54,
C62: 5..11 rounds) are single-slot or single-cell edges the seed reaches
rarely (a hole at the last slot of a full list, a re-read after
`Char_RecalcStats` whose byte the stir moves); each is still refused.

**A seeding trap met on the way**: the first control run left C60 standing
and C59 / C62 at 4 rounds, because `Inventory_Remove`'s `args` planted the
item into the list - and the harness captures the state *before* the
arguments, so every pass's `Apply` wiped the plant. The plant moved into
`Seed` (the arguments kept in `g_inv` for `args`), and C60 was refused (200
rounds), C59 443. Every control above is from the run after that change.
A group whose `args` writes memory has the same trap.

## 6. Latent defects (described, not fixed)

- **`Sprite_FlashClut` reads its own stack frame for a colour of 4 or
  more** (its four colours are locals indexed by the argument's byte):
  the return address's halves, the argument, then the caller's frame.
  Every caller seen passes a constant 0, 1, 2 or a byte out of
  `Field_FloorDamage`'s own table. **Ours aborts** with a message there.
- **`Camera_EaseAngleFB` divides by 0** when the speed x 4 (or x 8) is 0
  as a byte (a `Field_MoveSpeeds` entry of 0x40 or 0x20 / 0x40 at x 8) or
  its second argument is 0, and faults on -2^31 / -1. SC0's two calls pass
  10 and 15 (`push 0xA`, `push 0xF`). **Ours aborts.** A negative
  multiplier leaves a negative frame count that counts down through 2^32
  frames (reproduced).
- **`Char_LoseHp` tests the wrong record for low HP**: it hurts the member
  its argument names but sets the 0x2000 state bit from the HP of the
  record `Field_State +0x148` names, and marks `Field_State +0x90`. When
  `Field_Bit80Tick` hurts a member other than the one `Field_State` is
  (its bit-0x80 walk over the party list), the leader's HP decides.
  Reproduced; whether it shows is the owner's to see.
- **`Party_Remove` of an id not in a list** writes 0xFF at the list's slot
  `count` (the second list's slot 3 is `0x904068`, the first list's slot 3
  is the second list's slot 0); at a count of 0 it clears `+0` of the
  0x14C bytes before `ObjTrio` (`0x802BF4`) and passes
  `Member_ClearState(0xFF)`.
- **`Party_HealJoined` and `Party_PlaceForBattle` trust the member count**:
  above 3 they write `ObjTrio` records past the three (the heal copies a
  record to `+0x80` of each). Unchecked member ids index
  `MoveScript_EffectState` past its 24 (as `Party_Join` and SE's
  `Party_AddToLists` do).
- **`Char_LevelUp` indexes `CharacterRecords` and `Char_ExpTable` by a
  whole byte** (8 members).
- **`Inventory_Remove` reads the list pointers by a whole category byte**,
  as `Inventory_Add` does (D35's neighbourhood): above 4 it takes the
  count list's pointers and then the consumables' names as lists.
- **`MapView_FillCells` wraps only at the exact edge**: a `MapView_Row`
  above 0x37 or a column above 0x1B counts on past the table.

## 7. Found on the way

- **`Char_ExpTable`'s row bytes** `+2..+7` are the level-up's stat gains
  and two ability ids (the hypothesis in its `symbols.toml` entry, now
  read).
- **`scenario_rows.py`'s descent** stops at a forward `jmp` over a loop
  head (`0x56FCA0`, section 4).
- **`EventBattle_Records +1`** is the formation `Party_PlaceForBattle`
  reads (`0x532EE9`, as SE's entry noted), and `BattleFormation_Anims`
  `0x660B3C` / the pairs `0x660B1C` are indexed by it.
- **`0x904138`** is a second zenny dword `Zenny_Add` adds to unless its
  flag says not; `BattleResult_ZennyTick` passes the flag.
