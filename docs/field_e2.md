# Group FE2: the field engine's rest - the event script's helpers, the leader's hop helpers, the mode-11 object, seven field tail kinds, three map-cell draws, the trade screen

**Status:** MEASURED (2026-09-29) - round twelve
([`takeover-queue-field-battle.md`](takeover-queue-field-battle.md) section
3, [`takeover-queue-round12.md`](takeover-queue-round12.md)), wave two, the
field side, on the round branch's tip `61be26e`. **51 functions ours**
(`src/game/field_e2.cpp`, shadow name `field_e2`): the cut table's 44 rows for
FE2 (`analysis/round12_cut.tsv`) less `0x56D240` (a case of ours, section 5),
plus the eight starts of the band no list has - `0x536BF0`, `0x536EC0` (FH's
finding) and six `Field_ModeTailKinds` slots (section 5). Each read to its
last instruction with capstone and fuzzed through the scenario harness's
field mode ([`scenario_harness.md`](scenario_harness.md) section 7) without
edits to it: 306,000 rounds, 0 mismatches. 135 controls planted one at a time: 132 refused by a count, 2 by a crash (their near variants by a count), 1 equivalent (its near variant refused).
Fuzz-only except the three the owner's routes enter (section 9).

| Part | Functions | Reached through |
|---|--:|---|
| The event script's rest: `Scenario_CallB`, the after-battle tally and its two record counts, `Field_FaceMode11Object`, `Field_FloorHurt`, the wide cell tests, `Field_ObjectTrigger` and the two hooks under it | 13 | direct calls from ours (the chapter code, `GameMode_Field`, `Field_FloorDamage`, `AreaMap_CellsAll` / `None`, `Field_WayBlocked`, `Field_ObjectIdle`, `Field_LeaderTalkTest`), FC3 and FE1, and `Field_LeaderStates[14]` / `0x65F960[14]` |
| The leader's hop and step helpers `Leader_*` | 13 | FC3's leader states `0x525CE0..0x525F90`, FE1's turn states `0x52F970..0x52FB50`, Capcom's `0x51BD10` |
| The mode-11 object (the record at `0x905DA0` that `Game_Mode` 11 moves) and its draw | 8 | mode 11's step `0x496790`, `Mode11_ObjectStates` `0x660C2C`, FC3's `0x5172C0` |
| The field tail: `Field_ModeTailKinds` slots 4, 5, 6, 10, 27, 44, 55 | 7 | `Field_ModeTailRun` (ours) by the s8 `0x9039F3` |
| `PartySet_ErrorLoop`, `AreaMap_ClearCell` | 2 | `PartySet_Find`; 22 callers of the cell clear |
| Draw layers: `MapCell_Handlers` kinds 16, 33, 34 | 3 | `DrawLayer_Open` (ours) through `MapCell_Handlers` `0x663008` |
| The trade screen's states | 7 | `ItemTrade_States` `0x66A470` (dispatched by `0x593950`, nobody's), `ItemTrade_OpenSteps` `0x66A47C`, `ItemTrade_RunSteps` `0x66A484` |

Every name is from what the code does (`symbols.toml` status `evidence`
where the reading settles it, `hypothesis` where a name says more than the
code): "hop", "step up", "rise", "sink", "turn back" name the leader
helpers' shape - a jump set-up, a height compared with the ground ahead,
the height moved by 0x10 - not a play-tested move; "trade" names a list of
entries whose confirm adds an item (`Inventory_Add`) after a test of three
ingredient counts (`0x594700`) and a take (`0x594D90`); which screen of the
game it is was not traced. "Mode 11" is `Game_Mode` 11, which
`GameMode_Field`'s request 9 enters ([`mode-tasks.md`](mode-tasks.md)); what
the player sees in it is not established here.

## 1. What each function does

Every function's comment in `field_e2.cpp` is the full read and each
`symbols.toml` `evidence` string cites its extent; this section is the map.
SC is `Sprite_Current`, FS `Field_State`.

### 1.1 The event script's rest

| Function | Entry | Bytes | What |
|---|---|--:|---|
| `Scenario_CallB` | `0x5341C0` | 0x1B | entry `n & 0xFF` of `Scenario_CallBTables[Cond_ByteFA]`, a tail jump with the caller's arguments in place (naked, as `Scenario_CallA`) |
| `Field_AfterBattleTally` | `0x5341E0` | 0x1DD | `GameMode_Field`'s return from battle (`0x4961F2`): the byte `0x9045FB` counted to 0x1E under bits of `0x904654`; from chapter 8 the first clear story flag of 0xF6..0xFF set once `0x9046B0` reaches max(5, a stack threshold - a bias), counting pairs naming records 0xA / 0xB; then the two record counts, and story flag 0xF5 |
| `Records_CountUnpaired` | `0x5343C0` | 0x55 | `*counter + 1` unless a pair of the sixty at `0x9046D0` names a kind-4 record of the key |
| `Records_CountUnpairedOfKind` | `0x534420` | 0x57 | each record of the kind with `+3` set: its dword `+4` up unless a pair names it |
| `Field_FaceMode11Object` | `0x534480` | 0x10C | leader / member state 14: with `Field_Request` 9 the facing toward the mode-11 object by the signs of dx, dz (eight directions; both 0 keeps it), then `Sprite_EnsureAnimation`; else state 1 |
| `Field_FloorHurt` | `0x534C20` | 0x182 | a floor's damage by kind 0..8 from a 27-byte stack table: HP (`Char_LoseHp`) 4, 8, 16, 32, 2; AP (`0x537500`) 4, 8, 32 for 6..8; kinds 4 and 5 set FS `+0x124` / `+0x125`; the actor's state word at a quarter HP, its flags, `Actor_EquipCount(actor, 3, 8)` clearing bit 5 |
| `AreaMap_CellsAllWide` | `0x535490` | 0x172 | `AreaMap_CellsAll`'s wide footprint: up to nine cells all equal the code |
| `Field_WayBlockedWide` | `0x535830` | 0x415 | `Field_WayBlocked` for a raised sprite: `Field_CellsBlock(x, z, 1)`, up to 14 slope probes around the cell, the ground within 0xC0, `Sprite_ObjectAt` with the height set to the ground word |
| `AreaMap_CellsNoneWide` | `0x535D60` | 0x1E6 | the wide footprint's "none equals", with the 0x2n door filter |
| `Field_ObjectTrigger` | `0x56D6B0` | 0x3E | `+0x89` bit 6: `Field_ObjectTriggerByKind`; else the chapter's vtable slot 1; then `+0x86 = 0xFF` (the name of 2026-09-22 kept - verified) |
| `Scenario_CellHook` | `0x56D7A0` | 0x57 | the chapter's vtable slot 4 when set, its al not negative answering 1; else `Area_CellHook`; SC put back |
| `Field_ObjectTriggerByKind` | `0x56E020` | 0x1D | `call [0x662E1C + 4 * +0x86](object, 0x904030)`: `Field_ObjectTriggers[+0x86 - 1]` |

### 1.2 The leader's hop and step helpers

Each reads SC again after every call, as the originals do; their callers
test `al` where the table says "al".

| Function | Entry | Bytes | What |
|---|---|--:|---|
| `Leader_Pose3C` | `0x535FC0` | 0x1F | animation 0x3C facing 7, else 0x3D |
| `Leader_HopStart` | `0x535FE0` | 0x6A | the hop's set-up: FS `+0x128` = `+0x70 + 2`, `Field_JumpSetUp`, `+9` = (`+0x70` + 1) * `+9` * 2, `+0xA` = `+9` / 2, animation `+8 + 8`, a step, `+9 - 1` |
| `Leader_HopFlight` | `0x536050` | 0x69 | al: `+9` down with a step each frame, the CLUT word cleared half way; at 0 animation `+8`, `+0xA = 4`, al 1 |
| `Leader_HopPose` | `0x5360C0` | 0x64 | al: `+0xA` down; at 0 animation 0x37 / 0x34 and the facing reversed |
| `Leader_Pose3E` | `0x536130` | 0x31 | al: after the script tick, animation 0x3E / 0x3F |
| `Leader_StepUp` | `0x536170` | 0x115 | al: the ground ahead against the height less the actor's offset (`0x66978C`), the height by the rise (`0x6697A4`) or onto the ground, the view followed, a tick every fourth frame |
| `Leader_Pose34` | `0x536290` | 0x39 | the CLUT word cleared, animation 0x37 / 0x34 |
| `Leader_StepDown` | `0x5362D0` | 0xE5 | al: `Leader_StepUp` mirrored |
| `Leader_HopStartAfterTick` | `0x5363C0` | 0x7D | al: after the script tick, the hop's set-up |
| `Leader_HopFall` | `0x536440` | 0x8F | al: `+9` down with a step and the palette reloaded half way; at 0 the states reset |
| `Leader_Rise` | `0x5364D0` | 0x7B | the height up 0x10; at the ground ahead `+2 = 3`; tail `Sprite_ScriptTick` |
| `Leader_Sink` | `0x536550` | 0x80 | the height down 0x10; below the ground: onto it, the animation, the palette, the states reset; tail `Sprite_ScriptTick` |
| `Leader_TurnBack` | `0x5365D0` | 0x75 | the facing reversed, animation 0x3A / 0x3B, the palette, `+0x20 = -8`, states 2, 3, 3 |

### 1.3 The mode-11 object and the error screen

| Function | Entry | Bytes | What |
|---|---|--:|---|
| `PartySet_ErrorLoop` | `0x536A60` | 0x59 | never returns: a title line and the three ids `PartySet_Find` left, formatted into `0x904BA0`, drawn each frame |
| `Mode11_ObjectFrame` | `0x536B60` | 0x2F | `Field_InputHeld` from `Input_Held`, SC = `0x905DA0`, FS = `ObjTrio`, a jump through `Mode11_ObjectStates` by `+1` |
| `Mode11_ObjectStart` | `0x536B90` | 0x58 | state 0: the object placed at the leader, `+0xB` from the leader's flag byte |
| `Mode11_ObjectControl` | `0x536BF0` | 0x27C | state 1: bit 11 pressed - the kind-2 point moved to FS and the fall word by the distance (state 3); the leave buttons - `Game_Step` up; no direction - still; a step target - `Field_Request` 9; near the leader - a step (`Field_JumpStart`, `Field_JumpCheckHeight`, `Field_LeaderStepTick`), state 2 |
| `Mode11_ObjectMove` | `0x536E70` | 0x1C | state 2: `+9` down with `Field_LeaderStepTick`, at 0 `Mode11_ObjectHalt` |
| `Mode11_ObjectEnd` | `0x536E90` | 0x23 | state 3: once `Field_Kind2Hold` is 0, the view's elevation and `Field_Request` 0 |
| `Mode11_ObjectHalt` | `0x536EC0` | 0x4F | the velocity 0; a direction held: `Mode11_ObjectControl`, else state 1 |
| `Mode11_ObjectDraw` | `0x536F10` | 0x3C8 | a sprite marker (POLY_FT4 through `Gte_RotTransPers` and x87 / `_ftol`) and a 32-fan disc (POLY_G3 through `Gte_RotTransPers3`) under the object's matrix |

### 1.4 The field tail

Each is a state machine on the s8 `0x9039F4` with the argument `0x9039F5`
("cleared": kind, state and argument 0).

| Function | Slot | Entry | Bytes | What |
|---|--:|---|--:|---|
| `FieldTail_LoadBank` | 4 | `0x56D930` | 0xD8 | `LoadDatFile(0x31C)`, a CLUT row, `0x585A00`, `0x586670`, `Snd_LoadBankFile(0x2C2 + the set)`, cleared; state 5 waits on `Cond_ByteFE` 2 |
| `FieldTail_DropInMove` | 5 | `0x56DA10` | 0x164 | `Party_DropIn(0x662DE8[arg])`, a wait on the counter `0x90384B`, `Field_ChangeArea` by the argument (areas 0x6C, 0x22, 4), story flags 0x1E / 0x1F cleared |
| `FieldTail_Message` | 6 | `0x56DDD0` | 0x59 | script message 0xD8, then cleared |
| `FieldTail_HealAndMenu` | 10 | `0x56DB80` | 0x250 | fade, `Party_HealJoined`, a stream, fade in; the menu mode set up; a script message by the argument |
| `FieldTail_WorldMapHook` | 27 | `0x56DE30` | 0x11 | `WorldMap_FieldHooks[WorldMap_RecordIndex()]` |
| `FieldTail_FlagMessage` | 44 | `0x56DE50` | 0xBF | bit (arg) of `0x904650`, the system message `0x4172 + arg` into `Text_Records`, message 0xFA, stream 9 |
| `FieldTail_StoryWarp` | 55 | `0x56DF10` | 0x104 | message 0x20 / 0x21 by `Inventory_Count(0, 0x57, 0)`; then an area picked by the highest of story flags 0xF6..0xFF, the leader's place kept at `0x904148..` |

### 1.5 Draw layers and the cell clear

| Function | Kind | Entry | Bytes | What |
|---|--:|---|--:|---|
| `MapCell_DrawFrames` | 16 | `0x570870` | 0x190 | quads whose texture a `Frame_Counter` period and threshold bytes pick |
| `MapCell_DrawShaded` | 33 | `0x570BC0` | 0x214 | Gouraud quads (POLY_G4) behind draw modes |
| `MapCell_DrawSpinning` | 34 | `0x570DE0` | 0x2A4 | quads of 10-bit vertices under a rotation that turns with `Frame_Counter` |
| `AreaMap_ClearCell` | - | `0x5728D0` | 0x128 | the map byte cleared, the view cell's records of kinds 3, 9, 0x27 retyped 0x30 |

### 1.6 The trade screen

| Function | Entry | Bytes | What |
|---|---|--:|---|
| `ItemTrade_Open` | `0x593960` | 0x34 | `ItemTrade_States[0]`: the open step, the frame, the list |
| `ItemTrade_OpenStart` | `0x5939A0` | 0x2F | fade in, the pick 0, the quantity 1, the row count from `0x594790` |
| `ItemTrade_OpenWait` | `0x5939D0` | 0x19 | once the wait word is 0, state 1 |
| `ItemTrade_Run` | `0x5939F0` | 0x3A | `ItemTrade_States[1]`: the run step, the windows, the hand |
| `ItemTrade_PickItem` | `0x593A30` | 0x225 | the pick; confirm tests the ingredients and the count held (99: step 3, `0x594060`'s message) |
| `ItemTrade_PickCount` | `0x593C60` | 0x1F5 | the quantity by 1 / 10, held to 1..99 and to what the ingredients and the bag allow |
| `ItemTrade_Confirm` | `0x593E60` | 0x1F5 | yes: `Inventory_Add` and `0x594D90`; the item's name into `Text_Records` |

### 1.7 The PSX twins

`analysis/pairs_propagated.json` pairs 34 of the 51 (the event script's
helpers, the leader helpers, `PartySet_ErrorLoop`, `Mode11_ObjectFrame` and
`_ObjectDraw`, the object triggers and the cell hook, `AreaMap_ClearCell`,
the trade states; none for the tail kinds, the map-cell draws or the other
mode-11 states). Each twin's address is in its `symbols.toml` evidence as a
hypothesis. **The sibling names none of them** (`names/*.toml` and its
`symbols.toml`, searched 2026-09-29), so every name here is ours. For the two
wide cell tests the twins are psx_pair's swapped pairs (`0x801C4B70`,
`0x801C58F4`), as the evidence of `AreaMap_CellsAll4` / `None4` records. No
PSX code was read this round.

## 2. Divergence

One, extending an existing entry: **DIV-0023**. `MapCell_DrawFrames`,
`_DrawShaded`, `_DrawSpinning` and `Mode11_ObjectDraw` build SVECTORs on
their stacks (the quads' vertices; the spinning draw's angles and centre;
the marker's point, the disc's angles and translation) and never write the
fourth word; the GTE loaders copy whole dwords, so the stale stack reaches
the top halves of `Gte_Vertices`, which no instruction reads. Ours writes
`0000` there, the owner's ruling for that word (2026-09-21), and
DIVERGENCE.md's DIV-0023 has an "Also, 2026-09-29" paragraph naming them.
The fuzz hashes each SVECTOR's first six bytes only: the zero is by
construction.

Otherwise every function is a faithful replacement. Where the original
would index past a table, divide by 0 or loop for ever, ours aborts with a
message (the round-nine rule, no entry): section 6.

## 3. The arguments pushed with leftovers - the re-listed stand-ins

The standard stand-ins log every argument whole; the originals push whole
registers for bytes and words, whose upper bytes ours cannot reproduce.
Re-listed in `field_e2_fuzz.cpp` with what the callee reads (for the
harness fold):

| Callee | Mask | The read | Pushed by |
|---|---|---|---|
| `Sprite_EnsureAnimation` | u8 | `0x58933A cmp [+0x4B], al`; `0x589200 mov dl` | the leader helpers, `Field_FaceMode11Object` (ecx / eax with the pointer's bytes) |
| `Char_LoseHp` | u16, u8 | its amount's word compared and stored (`scena_sx.cpp`) | `Field_FloorHurt` (`movzx dx`, ecx's byte) |
| `0x537500` | u16, u8 | `0x537525 cmp cx, ax`; `and eax, 0xFF` | `Field_FloorHurt` |
| `Area_TestCondition` | u16 | its evidence: "callers push ax" | the three map-cell draws |
| `MapView_SetElevation` | u16 | `map_view.cpp`: the u16 offset and the s16 elevation | `Leader_StepUp` / `Down`, `Mode11_ObjectEnd` (dx after a call's clobber) |
| `Gfx_CommitPrim` | u8, u8 | its evidence: both arguments masked to a byte | `MapCell_DrawShaded` (al after `Gpu_SetDrawMode`) |
| `Msg_SystemPtr` | u16 | its evidence: `(u16 id)` | `FieldTail_FlagMessage` (dx after `Flags_Set`) |
| `Menu_DrawHand` | u16, u16, - | its evidence: a SPRT at (u16 x - 0x16, u16 y + 2) | `ItemTrade_Run` / `_Confirm` (13 * the s8 pick over a stale eax) |
| `Inventory_Count`, `Inventory_Add`, `Item_HelpMessage` | u8 each | `char_stats.cpp`: the low bytes | the trade states (a byte loaded into a register holding a count) |
| `0x594700` | u8, u8 | `0x594711 and ebp, 0xFF`; `0x594767 and ecx, 0xFF` | the trade states |

**The GTE stand-ins** were re-listed too, because a pointer into the
caller's stack frame is compared by value otherwise (the two passes' frames
differ): `Gte_RotTransPers` (the vertex hashed, its screen point and depth
not compared by value - and its screen point filled as **two floats**, eight
bytes: the standard `FxRotTransPers` fills four, and `Mode11_ObjectDraw`
reads the second float, so the standard stand-in left it stale and the two
sides differed), `Gte_RotTrans`, `Gte_RotMatrix`, `Gte_MulMatrix0`,
`Gte_SetRotMatrix`, `Gte_SetTransMatrix` (its translation, the matrix's
`+0x14`, noted), `Gte_RotTransPers3` (the depth pointer) and
`Gte_RotTransPers4` (the four vertices hashed without their fourth word,
DIV-0023; the depth pointer). **For the fold:** the field-standard `kField`
entries of these callees log stack pointers by value - fine for a caller
that passes its own globals (FH's thirteen), wrong for every function that
builds its vectors on its stack; `FxRotTransPers` wants eight bytes of
floats.

## 4. The fuzz (`field_e2_fuzz.cpp`)

**Clones:** `tools/band_rows.py --group FE2 --clones` (2026-09-29), every
extent read to its last instruction (the tool's, section 5), names given; the
three jump tables of the tail kinds relocated into their copies
(`FieldTail_HealAndMenu`'s byte index table is read in place, in the
original). **Shapes:** `kSprite` for the leader helpers and the mode-11
states that work on SC; `kState` for the tail kinds, the trade states,
`Mode11_ObjectFrame` / `_End` / `_Draw`, `Field_AfterBattleTally` and
`PartySet_ErrorLoop`; `kCall` for the helpers with arguments (a sprite
record for the object triggers, a scratch counter for
`Records_CountUnpaired`, a record buffer of the fuzz's own for the map-cell
draws); `kEntry` for `Scenario_CallB`. `ret_mask` 0xFF on every function
that answers in al (the wide cell tests, `Field_WayBlockedWide`, nine leader
helpers), 0xFFFFFFFF on `Scenario_CellHook` (its callers read eax).

**Stand-ins beyond the standard sets:** the group's own five (the two
record counts, the trigger by kind, `Mode11_ObjectHalt` / `_Control` as
`kPhase`); the re-listed callees of section 3; the non-standard
`0x537500`, `Party_HealJoined`, `Sound_StopMusic`, `Field_LeaderDirection`,
`Field_LeaderStepTarget`, `Field_JumpCheckHeight`, the CRT copy `0x5B9450`
(called for real: `kThrough`); **louder ones** where a caller branches on
the answer's value - `AreaMap_ByteAt` from an 8 x 8 grid the seed fills
(mostly the code, or the doors 0x2n), `Input_AutoRepeat` from the direction
bits (0x1000 .. 0x8000, the pairs), `WorldMap_RecordIndex` below 12,
`Inventory_Count` a byte of 99 at most (`ItemTrade_PickCount` loops for ever
past 227, section 6), `MapView_GroundAt` within 0xC0 of a seeded ground two
times in three (the step helpers compare it with the sprite's height,
`Field_WayBlockedWide` with 0xC0), `MapView_SlopeAt` at 0x3F..0x41 half
the time (the probes' `> 0x40`); **`Task_Sleep`** escaping
`PartySet_ErrorLoop` after one frame (on the original's side through the
copy's frame, 0x3C above the stand-in's return; on ours through a naked
wrapper's saved frame and registers); and **one typed stand-in per table
entry** called with arguments, built from the image at start-up - the call
tables B of chapters 6 and 9 (3 words), `Scenario_Hooks` slot 1 of chapters
1, 2, 6, 9 (1 word) and slot 4 of 1, 6, 9 (2 words; chapter 2's is 0, the
`Area_CellHook` path), `WorldMap_FieldHooks` (12) and
`Field_ObjectTriggers` (65) (2 words). 363 stand-ins registered.

**Tables swapped for recorders:** the 16 above, `Mode11_ObjectStates`,
`ItemTrade_OpenSteps`, `ItemTrade_RunSteps` (its fourth, `0x594060`,
nobody's, as a handler).

**Regions** beyond field mode's standard: the tail cells `0x9039A8..`, the
pairs' end and the eight records `0x904700..0x9048F0`, `CharacterRecords`
past the standard `0x903A14` region, the mode-11 object `0x905DA0`, the
CLUT words of member slots 0..3 `0x8113BE`, `Prim_VertexScratch`,
`MapView_Cells`, `MapView_Origin`, `Text_Records`, the trade bytes
`0x6BE080..` and states `0x939850..`, the button words `0x903580..`,
`Draw_OtSlot`, `Game_Mode` / `Game_Step`, `0x903800` (the pointer
`FieldTail_FlagMessage` writes through, aimed at a cell of the fuzz's),
`Cond_ByteFE`, and the fuzz's record, grid and ground buffers: 56 regions,
28,508 bytes.

**Seeds:** member slots 0..3 on the four sprite records (the CLUT words'
region), actor ids below 12 (the pace tables) and 8 (`CharacterRecords`),
facings 3 and 7 often (the pose tests); per function the boundaries each
reads - the tally's story flags set below a first clear one, the records'
kinds 4 / 5 / 0xB and pairs naming them, the thresholds at 0x13 / 0x14 /
0x27 / 0x3B, `0x9045FB` at 0x1D..0x1F; dx and dz 0, +1, -1 for the facing;
HP at a quarter of the maximum and one above; `+9` / `+0xA` equal for the
half-way tests; x and z fractions of 0 half the time for the wide tests;
the leader 0x60000 and one past from the mode-11 object; the chapter one of
1, 2, 6, 9 for the hooks; each tail kind's states, `Field_Request` 2, the
wait word 0, `0x90384B` at 0x20 / 0x31, `Game_Mode` 2, `Cond_ByteFE` 2;
well-formed map-cell records (a period not 0, the last threshold the
period, byte `+2` a subrecord boundary or 1 / 3); a run of records for the
cell clear built at the view cell the arguments name; the trade's pick,
row count, quantity at 0, 1, 0x62..0x64 and the buttons against
`Input_Pressed`. `disturb` moves `Field_State`, the slope flag `0x903850`,
the tail state and argument, the trade bytes, the object's state and SC
`+9`.

**Result (this worktree):** `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=field_e2`,
exit 0: **306,000 rounds over 51 functions (6,000 each), 2,217,320 calls to
the stand-ins, 0 MISMATCHES**; 28,508 bytes of state (56 regions) - the
final build, with the ground and slope stand-ins (the first full run, before
them, was 2,198,464 calls, 0 mismatches). `BOF3X_SHADOW='*'` on the same
build: exit 0, 970 self-test lines of 0 mismatches, `inject: 6604 ours, 0
left original` (6,553 + 51). Every handler of
the swapped tables was reached (the 17 call-table entries, the 7 hooks, the
12 world-map hooks, the 65 triggers, the four mode-11 states, the trade's
steps and `0x594060`). The subset switch `BOF3X_FE2_ONLY=first,count`
(the enum's numbering) and `BOF3X_FE2_ROUNDS` run part of the group - the
controls use them; the committed run is all 51 at 6,000.

**What stays thin:** `Field_WayBlockedWide`'s last test (`Sprite_ObjectAt`)
is reached in about one round in seven since the ground seed (11 in 6,000
before it); `Mode11_ObjectHalt` from `_Move` in half the rounds of `_Move`;
`Field_JumpStart` / `Field_JumpCheckHeight` (the near step of
`Mode11_ObjectControl`) in about 90 of 6,000.

## 5. What the cut and the tool said, settled

- **`0x56D240` is not a function**: it is case `'@'` of `Scena17_DrawLine`
  `0x56D1A0` (ours, `scena_sc15.cpp`: "`@` the logo, 88 pixels" - the logo
  drawn and x moved by 0x58), reached through the host's jump table
  `0x56D298`; the tool's `REFUSED +0x4B conditional jump out to 0x56d1df` is
  the case's jump back into its host. Ours holds its code; no `[[func]]`, no
  inject. ([`scena_sc15.md`](scena_sc15.md) found the same.)
- **Eight starts no list has** (the coordinator's decision: the band's are
  the group's): `0x536BF0` (`Mode11_ObjectStates[1]`, inside the catalog's
  736-byte `0x536B90`), `0x536EC0` (FH's: the tail jump at `0x536E7D`, inside
  the catalog's 127-byte `0x536E90`), and `Field_ModeTailKinds` slots 5, 10,
  6, 27, 44, 55 (`0x56DA10`, `0x56DB80`, `0x56DDD0`, `0x56DE30`, `0x56DE50`,
  `0x56DF10`, inside the catalog's 464-byte `0x56D930`). Each is reached by
  address (a table or a tail jump) and is taken as its own function.
- **Extents that differ from the cut in code**: `0x536B90` 0x58 (cut 736),
  `0x536E90` 0x23 (cut 127), `0x56D930` 0xD8 with its jump table (cut 464) -
  the tool's, read from the code, are right; `0x534420` 0x57 (cut 96) and 12
  more differ by padding only.
- **Hidden starts inside hosts already ours**: `0x56D930` (in
  `Field_CellTriggerAt` `0x56D800`'s catalog extent), `0x570870` (in
  `Area_TestCondition`'s), `0x570BC0` / `0x570DE0` (in `MapCell_FlatOverlay`'s),
  the seven trade states (in `Sprite_ClutWord`'s). None of those hosts'
  source holds the code: each ends at its own `ret` (`0x56D8AF`,
  `0x570017`, `0x570BBF`, `Sprite_ClutWord` at its jump table `0x593938`),
  and each start is reached through a table. `0x534480` is inside
  `0x534420` (FE2's own). `analysis/calltrace/entries_logic.txt` has the
  smaller extents (section 11).
- **The trade table's fourth run step `0x594060`** and the states' third
  `0x5940F0` (with its own table `0x66A494`) lie past the band's end: in no
  group, called through the tables in place. The dispatcher `0x593950`
  (`ItemTrade_States` by `0x93985C`, reached from the thunk `0x52CF30`) is in
  no cut row either.
- **Three `kStandard` / `kField` entries became ours**: `Scenario_CallB`
  (the harness's entry keys on the name and registers either way, as FH
  wrote it), and none other. `scena_sc11_fuzz.cpp` listed `Scenario_CallB` as
  Capcom's (`SC11_THEIRS`), which `Register` refuses once the name is ours: it
  now reads `SC11_OURS` (one line).
- **`ScenarioHarnessFh_Inject`** copies four of the thirteen functions it
  proves from the image - FE2's `0x5343C0`, `0x534420`, `0x56E020`,
  `0x5728D0` - so `FieldE2_Inject` runs after it in `inject_all.cpp` (last
  before `DrawPool_Grow`).

## 6. Latent defects (Capcom's, described, not fixed)

- **L1 `Scenario_CallB` in chapter 0**: `Scenario_CallBTables[0]` is 0, so a
  call-table-B call in chapter 0 jumps through `[n * 4]` - an access
  violation. Kept unchecked, as `Scenario_CallA` is (the naked jump cannot
  test and still hand the caller's arguments on); no chapter-0 code calls it.
- **L2 `Field_FloorHurt` past kind 8** reads past its 27-byte stack table
  into the return address; ours aborts. Unreachable: its callers pass a
  cell code - 0x80 for codes 0x80..0x88 only.
- **L3 `ItemTrade_PickCount`'s quantity loop** lowers an s8 quantity until
  it plus the item held is 99 or less; with more than 227 held no s8 value
  does and the original loops for ever (ours aborts). Unreachable in play: a
  stack holds 99 at most and the equipped count is under 9. The fuzz met it
  (the first full run hung, and clang had turned our endless loop into a
  silent exit 1) - the reason `Inventory_Count`'s stand-in answers a count.
- **L4 `MapCell_DrawFrames`' period 0** divides by 0 (ours aborts); its
  threshold scan has no bound, and a byte `+2` that no subrecord lands on
  never ends the walk (both kept, as `MapCell_DrawQuads`' walk is: they read
  the area's own records). The same walk-end condition holds for
  `MapCell_DrawShaded` and `_DrawSpinning`.
- **L5 `AreaMap_ClearCell`** writes `AreaMap_Bytes[x + z * width]` for any
  x, z - an x or z outside the map writes outside it; a view row or column
  past `MapView_Cells` (a `MapView_Row` / `Column` past their wraps) reads
  past the table (ours aborts); a record of 0 dwords, or a run whose count
  the steps overshoot, never ends the walk (ours aborts).
- **L6 `Field_ObjectTriggerByKind` at kind 0** calls `WorldMap_FieldHooks[11]`
  (`0x4139E0`, area 97's hook), the dword before `Field_ObjectTriggers`, with
  (object, flags): the table is read from 4 bytes below the triggers.
  Whether an object ever carries kind 0 with bit 6 of `+0x89` was not
  traced. Past kind 65 the reads run into `Area_CellHook`'s pairs (ours
  aborts where an entry is not code).
- **L7 `Mode11_ObjectFrame`** past state 3 jumps into data (ours aborts);
  the four states keep it below 4.
- `PartySet_ErrorLoop` never returning is the design (an error screen for a
  party with no set row), not a defect.

## 7. Calls across groups

FE2 calls no other wave-two group's function (`--edges`: every edge ends in
FE2), so it has no raw calls to rebind later. **Inbound from this wave**
(the coordinator's rebinding once both have merged):

| Caller group | Callers | FE2 callee |
|---|---|---|
| FC2 | `0x46D180` (four sites) | `AreaMap_ClearCell` |
| FC3 | `0x5172C0` | `Mode11_ObjectDraw` |
| FC3 | `0x526BA0` | `Field_FloorHurt` |
| FC3 | the leader states `0x525CE0..0x525F90` | `Leader_Pose3C`, `_HopStart`, `_HopFlight`, `_HopPose`, `_Pose3E`, `_StepUp`, `_Pose34`, `_StepDown`, `_HopStartAfterTick`, `_HopFall` |
| FE1 | `0x52F8F0` | `Field_ObjectTrigger` |
| FE1 | the turn states `0x52F970..0x52FB50` | the ten above and `Leader_Rise`, `_Sink`, `_TurnBack` |

**Inbound from outside the wave**: ours (rebound, section 8), Capcom's
(`0x51BD10`, `0x50572D`, 17 area functions calling `AreaMap_ClearCell`,
50 chapter functions calling `Scenario_CallB`), and the tables of section 4.

## 8. The rebinding

Every raw reference to an FE2 address in our source, the round-ten form
(the value unchanged; the fuzz keys stand):

| File | Constant | Now |
|---|---|---|
| `event_objs_callees.h` | `kCellsAllWide`, `kCellsNoneWide`, `kFloorHurt` | `bof3::addr::AreaMap_CellsAllWide`, `_CellsNoneWide`, `Field_FloorHurt` |
| `event_leader_callees.h` | `kWayBlockedWide` | `bof3::addr::Field_WayBlockedWide` |
| `event_ops_callees.h` | `kCellHook` | `bof3::addr::Scenario_CellHook` |
| `field_hidden_callees.h` | `kClearCell` | `bof3::addr::AreaMap_ClearCell` |
| `field_event.cpp` | `Fn<>(0x536A60)` in `kOriginals` | `Fn<>(bof3::addr::PartySet_ErrorLoop)` |
| `scena_sc3_callees.h` | `kCallB` | `bof3::addr::Scenario_CallB` |
| `scena_sc11_fuzz.cpp` | `SC11_THEIRS(Scenario_CallB)` | `SC11_OURS` (it would no longer register) |

The four `_callees.h` headers include `bof3/symbols.gen.h` now (one line
each). **Left raw on purpose:** the fuzz files' keys - `CallSite` tables and
stand-in maps naming `0x5341C0` (`scena_sc1`, `sc2`, `sc3`, `sc5`, `sc9a`,
`sc9b`, `sc11`, `sc12`, `sc13` fuzz files), `0x535830`
(`event_leader_fuzz.cpp`), `0x56D7A0` (`event_ops_fuzz.cpp`), `0x5728D0`
(`field_hidden_fuzz.cpp`), `0x536A60` (`field_event_fuzz.cpp`), `0x56D6B0`
(`object_kinds.cpp`'s stub map); comments; `scenario_harness.cpp`'s and
`scenario_harness_fh.cpp`'s rows (harness files, not a group's to edit).
`object_kinds.cpp` names `Field_ObjectTrigger` in its callee table and now
reaches ours directly (the name is ours).

## 9. The live route

The owner's two routes, first call of each start (`reach_whelp`,
`reach_dragon`, 2026-09-29 at `979a567`, all original):

| Function | `whelpBoss.txt` | `dragonTransform.txt` |
|---|---|---|
| `Field_AfterBattleTally` | frame 1077 (from `0x4961F7`) | frame 4227 |
| `Records_CountUnpaired` | - | frame 4227 (from `0x534374`) |
| `Records_CountUnpairedOfKind` | - | frame 4227 (from `0x53437E`) |

The coordinator's frame-hash A/B after the wave covers these three (each
route returns from a battle; the dragon route's chapter is 8 or more).
**The other 48 are fuzz-only.**

## 10. Controls

135 controls planted one at a time (`controls.py` in the session scratchpad: anchor, rebuild, `BOF3X_FE2_ONLY` = the function, 3,000 rounds, restore, rebuild): **132 refused by a count**, 2 refused by a crash of ours (each with a near variant refused by a count), 1 equivalent (its near variant refused). `W1` was not refused on the first pass - the slope stand-in never answered 0x40 - and was refused after `MapView_SlopeAt` was made to answer at the boundary (the table has the re-run).

| Id | Function | Planted | Result |
|---|---|---|---|
| C1 | `Scenario_CallB` | CallB: the index and 6 | refused in 1,611 of 3,000 rounds |
| T1 | `Field_AfterBattleTally` | Tally: the byte capped at 0x1D | refused in 128 of 3,000 rounds |
| T2 | `Field_AfterBattleTally` | Tally: the threshold floor 6 | refused in 68 of 3,000 rounds |
| T3 | `Field_AfterBattleTally` | Tally: the first flags take the 0xB count | refused in 8 of 3,000 rounds |
| T4 | `Field_AfterBattleTally` | Tally: key 1 counted below 8 | refused in 345 of 3,000 rounds |
| T5 | `Field_AfterBattleTally` | Tally: kind 0xA counted | refused in 1,784 of 3,000 rounds |
| U1 | `Records_CountUnpaired` | CountUnpaired: counted at one pair | refused in 2,665 of 3,000 rounds |
| U2 | `Records_CountUnpaired` | CountUnpaired: kind 5 | refused in 759 of 3,000 rounds |
| K1 | `Records_CountUnpairedOfKind` | OfKind: +2 for +3 | refused in 1,715 of 3,000 rounds |
| K2 | `Records_CountUnpairedOfKind` | OfKind: counted at one pair | refused in 546 of 3,000 rounds |
| F1 | `Field_FaceMode11Object` | Face: (-, 0) 5 | refused in 242 of 3,000 rounds |
| F2 | `Field_FaceMode11Object` | Face: request 8 | refused in 2,235 of 3,000 rounds |
| F3 | `Field_FaceMode11Object` | Face: (0, -) 2 | refused in 247 of 3,000 rounds |
| H1 | `Field_FloorHurt` | FloorHurt: kind 4 flags 0x40 | refused in 322 of 3,000 rounds |
| H2 | `Field_FloorHurt` | FloorHurt: bit 4 cleared | refused in 1,568 of 3,000 rounds |
| H3 | `Field_FloorHurt` | FloorHurt: a quarter exactly not counted | refused in 371 of 3,000 rounds |
| H4 | `Field_FloorHurt` | FloorHurt: EquipCount value 9 | refused in 3,000 of 3,000 rounds |
| A1 | `AreaMap_CellsAllWide` | CellsAll: (x-1, z+1) not read | refused in 613 of 3,000 rounds |
| A2 | `AreaMap_CellsAllWide` | CellsAll: the corner with an x fraction of 0 only | refused in 259 of 3,000 rounds |
| A3 | `AreaMap_CellsAllWide` | Cells: the mask 0xE0 | refused in 350 of 3,000 rounds |
| W1 | `Field_WayBlockedWide` | WayBlocked: slope 0x40 counts | refused in 104 of 3,000 rounds |
| W2 | `Field_WayBlockedWide` | WayBlocked: 0xBF | refused in 2 of 3,000 rounds |
| W3 | `Field_WayBlockedWide` | WayBlocked: the corner probe direction 2 | refused in 162 of 3,000 rounds |
| W4 | `Field_WayBlockedWide` | WayBlocked: the height put back through the old pointer | refused in 16 of 3,000 rounds |
| W5 | `Field_WayBlockedWide` | WayBlocked: the last z probe at z + 2 | refused in 312 of 3,000 rounds |
| N1 | `AreaMap_CellsNoneWide` | CellsNone: doors and 0x31 | not refused: equivalent (a 0x2n cell has bit 4 clear); near variant N1b refused |
| N2 | `AreaMap_CellsNoneWide` | CellsNone: the corner filtered | refused in 7 of 3,000 rounds |
| P1 | `Leader_Pose3C` | Pose3C: facing 6 | refused in 1,138 of 3,000 rounds |
| S1 | `Leader_HopStart` | HopSetUp: pace + 1 | refused in 3,000 of 3,000 rounds |
| S2 | `Leader_HopStart` | HopSetUp: +9 times 4 | refused in 2,939 of 3,000 rounds |
| S3 | `Leader_HopStart` | HopSetUp: animation +7 | refused in 3,000 of 3,000 rounds |
| L1 | `Leader_HopFlight` | HopFlight: +0xA 5 | refused in 645 of 3,000 rounds |
| L2 | `Leader_HopFlight` | HopFlight: the CLUT word off the half way | refused in 2,355 of 3,000 rounds |
| O1 | `Leader_HopPose` | HopPose: 6 - n | refused in 716 of 3,000 rounds |
| O2 | `Leader_HopPose` | HopPose: 0x36 | refused in 233 of 3,000 rounds |
| O3 | `Leader_HopPose` | HopPose: +9 = 2 | refused in 2,226 of 3,000 rounds |
| E1 | `Leader_Pose3E` | Pose3E: 0x3E always | refused in 1,344 of 3,000 rounds |
| E2 | `Leader_Pose3E` | Pose3E: al 1 on no tick | refused in 1,028 of 3,000 rounds |
| V1 | `Leader_StepUp` | StepUp: the rise added | refused in 1,639 of 3,000 rounds |
| V2 | `Leader_StepUp` | StepUp: one lower | refused in 1,230 of 3,000 rounds |
| V3 | `Leader_StepUp` | StepUp: the actor +0x8A | refused in 1,764 of 3,000 rounds |
| V4 | `Leader_StepUp` | FollowAndTick: every eighth frame | refused in 1,010 of 3,000 rounds |
| V5 | `Leader_StepUp` | FollowAndTick: bit 2 | refused in 951 of 3,000 rounds |
| Q1 | `Leader_Pose34` | Pose34: the CLUT word 1 | refused in 3,000 of 3,000 rounds |
| D1 | `Leader_StepDown` | StepDown: equal heights step | refused in 4 of 3,000 rounds |
| D2 | `Leader_StepDown` | StepDown: the first ground kept | refused in 1,148 of 3,000 rounds |
| G1 | `Leader_HopStartAfterTick` | HopAfterTick: the tick inverted | refused in 3,000 of 3,000 rounds |
| B1 | `Leader_HopFall` | HopFall: +3 = 1 | refused in 645 of 3,000 rounds |
| B2 | `Leader_HopFall` | HopFall: the palette off the half way | refused in 2,355 of 3,000 rounds |
| R1 | `Leader_Rise` | Rise: up 0x11 | refused in 3,000 of 3,000 rounds |
| R2 | `Leader_Rise` | Rise: at the floor not | refused in 3 of 3,000 rounds |
| K3 | `Leader_Sink` | Sink: down 0xF | refused in 1,160 of 3,000 rounds |
| K4 | `Leader_Sink` | Sink: +2 = 1 | refused in 1,860 of 3,000 rounds |
| X1 | `Leader_TurnBack` | TurnBack: +0x20 = -16 | refused in 3,000 of 3,000 rounds |
| X2 | `Leader_TurnBack` | TurnBack: 0x3C | refused in 2,049 of 3,000 rounds |
| Y1 | `PartySet_ErrorLoop` | ErrorLoop: y 0x75 | refused in 3,000 of 3,000 rounds |
| Y2 | `PartySet_ErrorLoop` | ErrorLoop: the ids swapped | refused in 2,986 of 3,000 rounds |
| M1 | `Mode11_ObjectFrame` | ObjectFrame: held + 1 | refused in 3,000 of 3,000 rounds |
| M2 | `Mode11_ObjectFrame` | ObjectFrame: the second record | refused in 3,000 of 3,000 rounds |
| M3 | `Mode11_ObjectStart` | ObjectStart: +0x3C from z | refused in 3,000 of 3,000 rounds |
| M4 | `Mode11_ObjectControl` | ObjectControl: divisor 0x21 | refused in 732 of 3,000 rounds |
| M5 | `Mode11_ObjectControl` | ObjectControl: the pace swapped | refused in 75 of 3,000 rounds |
| M6 | `Mode11_ObjectControl` | ObjectControl: x at 0x60000 far | refused in 36 of 3,000 rounds |
| M7 | `Mode11_ObjectControl` | ObjectControl: >> 12 | refused in 724 of 3,000 rounds |
| M8 | `Mode11_ObjectMove` | ObjectMove: +9 - 2 | refused in 1,451 of 3,000 rounds |
| M9 | `Mode11_ObjectEnd` | ObjectEnd: request 1 | refused in 1,540 of 3,000 rounds |
| M10 | `Mode11_ObjectHalt` | ObjectHalt: bit 15 not held | refused in 98 of 3,000 rounds |
| M11 | `Mode11_ObjectHalt` | ObjectHalt: +9 cleared | refused in 2,978 of 3,000 rounds |
| Z1 | `Mode11_ObjectDraw` | ObjectDraw: v 0x9C | refused in 1,168 of 3,000 rounds |
| Z2 | `Mode11_ObjectDraw` | ObjectDraw: the radius * 3 | refused in 1,114 of 3,000 rounds |
| Z3 | `Mode11_ObjectDraw` | ObjectDraw: x - 7 | refused in 2,245 of 3,000 rounds |
| Z4 | `Mode11_ObjectDraw` | ObjectDraw: y signed, not a qword | refused in 1,126 of 3,000 rounds |
| Z5 | `Mode11_ObjectDraw` | ObjectDraw: the half by a shift | refused in 557 of 3,000 rounds |
| Z6 | `Mode11_ObjectDraw` | ObjectDraw: y from the x float | refused in 2,256 of 3,000 rounds |
| J1 | `Field_ObjectTrigger` | Trigger: +0x86 0xFE | refused in 3,000 of 3,000 rounds |
| J2 | `Field_ObjectTrigger` | Trigger: bit 5 | refused in 1,500 of 3,000 rounds |
| J3 | `Field_ObjectTrigger` | Trigger: slot 2 | refused in 1,502 of 3,000 rounds |
| I1 | `Scenario_CellHook` | CellHook: 0 not an answer | refused in 9 of 3,000 rounds |
| I2 | `Scenario_CellHook` | CellHook: Sprite_Current not put back | refused in 56 of 3,000 rounds |
| I3 | `Scenario_CellHook` | CellHook: slot 3 tested | refused by a crash of ours (the planted bug calls address 0 / walks past the record); a count variant below |
| L3 | `FieldTail_LoadBank` | TailLoad: file 0x31D | refused in 412 of 3,000 rounds |
| L4 | `FieldTail_LoadBank` | TailLoad: bank 0x2C3 | refused in 455 of 3,000 rounds |
| L5 | `FieldTail_LoadBank` | TailLoad: Cond_ByteFE 3 | refused in 216 of 3,000 rounds |
| L6 | `FieldTail_DropInMove` | TailDropIn: timer 0x11 | refused in 161 of 3,000 rounds |
| L7 | `FieldTail_DropInMove` | TailDropIn: 0x30 | refused in 200 of 3,000 rounds |
| L8 | `FieldTail_DropInMove` | TailDropIn: argument 5 no area | refused in 17 of 3,000 rounds |
| L9 | `FieldTail_HealAndMenu` | TailHeal: 0x16 | refused in 121 of 3,000 rounds |
| L10 | `FieldTail_HealAndMenu` | TailHeal: state 11 nothing | refused in 47 of 3,000 rounds |
| L11 | `FieldTail_HealAndMenu` | TailHeal: pass flags 0x1E | refused in 170 of 3,000 rounds |
| L12 | `FieldTail_HealAndMenu` | TailHeal: Game_Mode 3 | refused in 66 of 3,000 rounds |
| L13 | `FieldTail_Message` | TailMessage: 0xD9 | refused in 1,291 of 3,000 rounds |
| L14 | `FieldTail_WorldMapHook` | TailWorld: the index & 7 | refused in 977 of 3,000 rounds |
| L15 | `FieldTail_FlagMessage` | TailFlag: 0x4173 | refused in 583 of 3,000 rounds |
| L16 | `FieldTail_FlagMessage` | TailFlag: the byte 1 | refused in 392 of 3,000 rounds |
| L17 | `FieldTail_FlagMessage` | TailFlag: 7 bytes copied | refused in 583 of 3,000 rounds |
| L18 | `FieldTail_StoryWarp` | TailWarp: area step | refused in 103 of 3,000 rounds |
| L19 | `FieldTail_StoryWarp` | TailWarp: flag 0x20 | refused in 235 of 3,000 rounds |
| L20 | `FieldTail_StoryWarp` | TailWarp: z kept from x | refused in 339 of 3,000 rounds |
| Q2 | `Field_ObjectTriggerByKind` | TriggerByKind: flags + 1 | refused in 3,000 of 3,000 rounds |
| FR1 | `MapCell_DrawFrames` | DrawFrames: skip + 4 | refused by a crash of ours (the planted bug calls address 0 / walks past the record); a count variant below |
| FR2 | `MapCell_DrawFrames` | DrawFrames: the threshold equal stops | refused in 167 of 3,000 rounds |
| FR3 | `MapCell_DrawFrames` | DrawFrames: y0 one cell off | refused in 1,678 of 3,000 rounds |
| FR4 | `MapCell_DrawFrames` | QuadVertex: y from byte 1 | refused in 1,678 of 3,000 rounds |
| SH1 | `MapCell_DrawShaded` | DrawShaded: tpage 0x94 | refused in 1,672 of 3,000 rounds |
| SH2 | `MapCell_DrawShaded` | DrawShaded: green >> 9 | refused in 1,672 of 3,000 rounds |
| SH3 | `MapCell_DrawShaded` | DrawShaded: semi >> 30 | refused in 1,484 of 3,000 rounds |
| SH4 | `MapCell_DrawShaded` | DrawShaded: the last draw mode dtd 1 | refused in 1,997 of 3,000 rounds |
| SP1 | `MapCell_DrawSpinning` | DrawSpinning: x sign 0xF800 | refused in 1,430 of 3,000 rounds |
| SP2 | `MapCell_DrawSpinning` | DrawSpinning: slot 7 | refused in 1,175 of 3,000 rounds |
| SP3 | `MapCell_DrawSpinning` | DrawSpinning: angle y & 0x7FF | refused in 1,204 of 3,000 rounds |
| SP4 | `MapCell_DrawSpinning` | DrawSpinning: x bits 10, 11 dropped | refused in 1,565 of 3,000 rounds |
| SP5 | `MapCell_DrawSpinning` | DrawSpinning: the centre z from the high word | refused in 1,672 of 3,000 rounds |
| CC1 | `AreaMap_ClearCell` | ClearCell: kind 10 | refused in 112 of 3,000 rounds |
| CC2 | `AreaMap_ClearCell` | ClearCell: row + 2 | refused in 225 of 3,000 rounds |
| CC3 | `AreaMap_ClearCell` | ClearCell: kind 0x31 | refused in 225 of 3,000 rounds |
| CC4 | `AreaMap_ClearCell` | ClearCell: the byte 1 | refused in 3,000 of 3,000 rounds |
| IT1 | `ItemTrade_Open` | TradeOpen: frame before cursor | refused in 3,000 of 3,000 rounds |
| IT2 | `ItemTrade_OpenStart` | TradeOpenStart: quantity 2 | refused in 2,978 of 3,000 rounds |
| IT3 | `ItemTrade_OpenWait` | TradeOpenWait: state 2 | refused in 1,517 of 3,000 rounds |
| IT4 | `ItemTrade_Run` | TradeRun: hand y 0x50 | refused in 2,029 of 3,000 rounds |
| IT5 | `ItemTrade_Run` | TradeRun: bit 6 draws | refused in 299 of 3,000 rounds |
| IT6 | `ItemTrade_PickItem` | TradePick: wrap to count - 2 | refused in 473 of 3,000 rounds |
| IT7 | `ItemTrade_PickItem` | TradePick: 99 held allowed | refused in 1 of 3,000 rounds |
| IT8 | `ItemTrade_PickItem` | TradePick: step 2 on full | refused in 113 of 3,000 rounds |
| IT9 | `ItemTrade_PickItem` | TradePick: cancel answer 0 | refused in 545 of 3,000 rounds |
| IT10 | `ItemTrade_PickCount` | TradeCount: -11 | refused in 149 of 3,000 rounds |
| IT11 | `ItemTrade_PickCount` | TradeCount: cap 0x62 | refused in 457 of 3,000 rounds |
| IT12 | `ItemTrade_PickCount` | TradeCount: the sound inverted | refused in 3,000 of 3,000 rounds |
| IT13 | `ItemTrade_Confirm` | TradeConfirm: sound 0x104 | refused in 120 of 3,000 rounds |
| IT14 | `ItemTrade_Confirm` | TradeConfirm: hand step 35 | refused in 393 of 3,000 rounds |
| IT15 | `ItemTrade_Confirm` | TradeConfirm: the names at step 1 | refused in 1,497 of 3,000 rounds |
| IT16 | `ItemTrade_Confirm` | TradeConfirm: 12 bytes of the name | refused in 745 of 3,000 rounds |
| N1b | `AreaMap_CellsNoneWide` | CellsNone: doors and 0x23 (near variant of N1) | refused in 108 of 3,000 rounds |
| I3b | `Scenario_CellHook` | CellHook: the hook answer 2 | refused in 1,147 of 3,000 rounds |
| FR1b | `MapCell_DrawFrames` | DrawFrames: the next texture word | refused in 1,678 of 3,000 rounds |

**2026-10-05, under the repaired disturbance (round fourteen's review item
1).** `Move` switched on `h % 9` and is reached only with `h % 3 != 0`, so its
cases 0 (`Field_State` to another ObjTrio record), 3 (the trade pick
`0x6BE08C`) and 6 (the object's state `0x905DA1`) never ran; `b9dfe34` draws
the case through `sh::DisturbCase`. The same script re-run on `451edeb`, all
148 anchors still found once (none repaired): **135 planted, 131 refused by a
count**, I3 and FR1 refused by a crash / an abort of ours as before (exit
`0xC0000005`; `MapCell_DrawFrames: a frame period of 0`), N1 not refused
(the equivalent mutant, as before). 16 counts are the table's, 115 moved
(the generator's stream changed). **D1 was refused in 4 and is refused in 0
of 3,000 now**; it is not blind, only thin - an equal height and ground is a
boundary the ground stand-in hits about once in 1,000 rounds, which the
disturbance does not touch: 4 of 6,000 (the committed run's rounds) and 30
of 30,000 (`BOF3X_FE2_ROUNDS`). The fuzz was not changed. W2 1 (was 2), R2 1
(was 3) and IT7 3 (was 1) are the other counts below 5.

New controls for the formerly dead cases, each also run against the old
switch (a temporary `BOF3X_DISTURB_OLD=1` toggle in the fuzz, `h % 9`, not
committed). Nothing else moves `Field_State` or the pick (the harness's cases
do not), and **every one is 0 under the old switch**: these re-reads were
untested before. DS5 and DS8 are refused in 1 of 6,000 and 6 of 30,000, not
at 3,000. **Case 6 is noise for this group**: the object's state `+1` is read
only by `Mode11_ObjectFrame` (before any call) and `Mode11_ObjectStart`
(which calls nothing); after a call the group only writes it. No control was
planted for it.

| Id | Function | Planted | Result |
|---|---|---|---|
| DS1 | `Field_FloorHurt` | case 0: the second HP call's actor +0x89 not re-read after Char_LoseHp | refused in 12 of 3,000 rounds (old switch 0) |
| DS2 | `Field_FloorHurt` | case 0: Field_State not re-read after Actor_EquipCount | refused in 7 of 3,000 rounds (old switch 0) |
| DS3 | `Leader_StepUp` | case 0: the actor +0x89 not re-read after the second MapView_GroundAt | refused in 4 of 3,000 rounds (old switch 0) |
| DS4 | `Leader_StepDown` | case 0: the actor +0x89 read before MapView_GroundAt | refused in 4 of 3,000 rounds (old switch 0) |
| DS5 | `Leader_Rise` | case 0: the actor +0x89 read before MapView_GroundAt | not refused in 3,000; 1 of 6,000, 6 of 30,000 (old switch 0) |
| DS6 | `Mode11_ObjectControl` | case 0: the pace stored through the Field_State read on entry | refused in 2 of 3,000 rounds (old switch 0) |
| DS7 | `ItemTrade_Run` | case 3: the pick read before the frame and the cursor | refused in 18 of 3,000 rounds (old switch 0) |
| DS8 | `ItemTrade_PickItem` | case 3: the record by the pick read before 0x594700 | not refused in 3,000; 1 of 6,000, 6 of 30,000 (old switch 0) |
| DS9 | `ItemTrade_PickItem` | case 3: bit 6 set on the pick read before the two Inventory_Count | refused in 1 of 3,000 rounds (old switch 0) |
| DS10 | `ItemTrade_PickItem` | case 3: the help line's pick read before the box | refused in 5 of 3,000 rounds (old switch 0) |
| DS11 | `ItemTrade_PickCount` | case 3: bit 6 cleared on the pick read before the checks and counts | refused in 12 of 3,000 rounds (old switch 0) |
| DS12 | `ItemTrade_PickCount` | case 3: the entry k from the pick read before Input_AutoRepeat | refused in 8 of 3,000 rounds (old switch 0) |

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (append only, 8,163 -> 8,187 lines): the
23 starts it did not list, with the extents above, under a
`# round twelve group FE2` comment. 25 starts were listed with the same
extent. Three were listed with a larger one and left for the merger:
`00534420 16C` (really 0x57; `0x534480` follows as its own line),
`00536B60 3AF` (really 0x2F; `0x536B90`, `0x536BF0`, `0x536E70`,
`0x536E90`, `0x536EC0` follow), `0056E020 2A` (0x1D and padding).
