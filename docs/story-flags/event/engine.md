# Event flags: the engine

**Status:** IN PROGRESS (2026-11-09, read from source; not checked in play).

The event flags are the bit bank at `0x904030` (`Cond_Flags + 0xA0`, save block
`+0x650`, 40 bytes up to `Party_Zenny` `0x904058`). This file covers the engine
files of `src/game/` (not `area_*`, `effect_*`, `boss_*`, `rest_*`, `scena_*`).
Its main result is the generic paths: the ones where the flag number comes from
the game's data, so data scripts can test or change a flag that no C++ code names.

All paths below go through `Flags_Set` / `Flags_Clear` / `Flags_Test`
(`event_script.cpp:679`, `:685`, `:693`). Each masks the index to 8 bits
(`index & 0xFF`), so through one bank pointer a call reaches flags 0..255 only.
Event flags 256..319 (bytes `0x904050..0x904057`) are reachable only through a
row pointer above `Cond_Flags + 0xA0` (rows `0x15..0x18`, below), or by a direct
byte access. Reading, not checked.

## Data-driven mechanisms

| Mechanism | Function | Bank | Flag number from | Op |
|---|---|---|---|---|
| Event-script condition 12 | `EventCond_StoryFlag`, `field_o.cpp:1303` | event flags, fixed | operand byte after the condition index | test |
| Event-script condition 2 | `EventCond_Flag`, `event_script.cpp:706` | row `EventScript_FlagBank` of `Cond_Flags`; row 20 is the event bank | operand byte after the condition index; row from op `F9 b` | test |
| Area condition word | `Area_TestCondition`, `map_cells.cpp:81` | row `kind` (high byte) of `Cond_Flags`; kind `0x14` is the event bank | low byte of the 16-bit code | test (kinds `0x20..0x3F` inverted) |
| Zone music sets | `Area_ZoneTrack`, `window_task.cpp:216` | row `entry[1]` of `Cond_Flags` | `entry[2]` of a 3-byte entry of the table at `0x669A48` | test |
| Movement-script variable 12 | `MoveScript_Variable`, `move_cmds.cpp:436`, read by `MoveScript_Flow` ops `04..09` | event flags 0..31, as one dword | the op's mask or compare operand | test (compare) |
| Object trigger by kind | `Field_ObjectTriggerByKind`, `field_e2.cpp:1064` | event bank pointer handed to a handler | the handler, chosen by object byte `+0x86` | unknown here |
| New Game seed | `Scenario_SubInit`, `title_states.cpp:440` | event flags 0..127 | 16 bytes of `Scenario_SubInitTable` `0x662CD8` | write (set and clear) |

No event-script op handler (`EventOp_*`) and no movement-script command in the
engine files sets or clears an event flag. The data can only **test** event
flags through the generic paths. Every set or clear in the engine names its flag
in code. Reading, not checked: the area, scenario and rest files may hold more
generic setters (for example the object-trigger handlers, below).

### 1. Event-script condition 12: `EventCond_StoryFlag`

- `field_o.cpp:1303-1305`: `Flags_Test(0x904030, **at)`. The bank is fixed at the
  event flags. The flag number is the byte at the script position the condition
  is given.
- It is entry 12 of `EventScript_Conditions` `0x663B30` (comment at
  `field_o.cpp:1301`, original `0x57C270`). The interpreter reaches it in two ways:
  - `F0 c x` / `F1 c x` (if / if-not): `If`, `event_script.cpp:153-157`. `c` is
    the condition index, masked to 5 bits. The condition reads `x`.
  - `F4 c { F6 x ... }` (switch): `EventScript_Switch`, `event_script.cpp:230-240`.
    The condition `c` (signed, not masked) is tested again with each `F6 x` label's `x`.
- So `F0 0C x` runs its first arm when event flag `x` is set, and `F1 0C x` when
  it is clear. Flags 0..255.
- The scripts are the area's placement script (`Area_RunPlacement`, area
  descriptor `+0`) and talk scripts (descriptor `+4` table), all through
  `EventScript_Run` (`symbols.toml:4319`).

### 2. Event-script condition 2: `EventCond_Flag`

- `event_script.cpp:706-708`:
  `Flags_Test(Cond_Flags + (EventScript_FlagBank & 0xFF) * 8, **at)`.
- `EventScript_FlagBank` is set to `Cond_ByteFA` (the chapter, sign-extended) at
  the start of every run (`EventScript_Run`, `event_script.cpp:111`). Op `F9 b`
  sets it to `b` (`event_script.cpp:133-135`). Op `FA` sets it back to the
  chapter (`event_script.cpp:136-138`).
- So by default condition 2 tests the **current chapter's** row. After `F9 14`
  it tests the event flags, with the flag number from the operand byte. After
  `F9 15..18` it tests 8-byte rows that lie inside the event bank, and `F9 18`
  reaches event flags 256..319. Reading, not checked: whether any shipped script
  uses `F9 14` or above is not known here.

### 3. Area condition word: `Area_TestCondition`

- `map_cells.cpp:81-96`. The 16-bit code's high byte is the kind, the low byte
  the operand (comment `map_cells.cpp:69-79`).
  - Kind `0x00..0x1F`: `Flags_Test(Cond_Flags + 8 * kind, code)`
    (`map_cells.cpp:93`). Kind `0x14` is the event bank. Kinds `0x15..0x18`
    are rows inside it.
  - Kind `0x20..0x3F`: the same test of row `kind & 0x1F`, inverted
    (`map_cells.cpp:94`). Kind `0x34` is "event flag clear".
  - Other kinds test other bytes (`0xFA..0xFF`) or return `code & 1`.
- The code comes from records in the area's map data. Callers in the engine:
  - map cells: `DrawQuads` (`map_cells.cpp:171`), `MapCell_DrawFrames`
    (`field_e2.cpp:1476`), `MapCell_DrawShaded` (`:1515`), `MapCell_DrawSpinning` (`:1566`),
    `MapCell_DrawAnimated` (`map_field_objects.cpp:350`).
  - battle-scene cells: `MapCell_DrawTexQuads` (`battle_e6.cpp:1015`),
    `MapCell_DrawShadedQuads` (`:1058`), `MapCell_DrawSpinQuads` (`:1102`),
    `MapCell_DrawGroundSprite` (`:1161`).
  - the area patch list, `AreaMap_ApplyPatch` (`field_misc.cpp:370`). The
    condition picks one of two textures, corner sets or cell bytes for each record.
  - the area tint `AreaMap_SetupTint` (`map_field_objects.cpp:401`), the palette
    cycle `AreaMap_ClutCycle` (`map_layers.cpp:702`), and the texture cycle
    `AreaMap_TextureCycle` (`area_backdrop.cpp:218`).
- So an event flag can change what an area's map shows. The flag number is in
  the map data. Op: test only.

### 4. Zone music sets: `Area_ZoneTrack`

- `window_task.cpp:216-224` (comment `:180-189`). When a zone record's byte `+7`
  is not 0, byte `+6` picks an 8-byte record of the table `kMusicSets` `0x669A48`
  (`window_task_callees.h:58`). That record points at `count` 3-byte entries
  (track, row, flag). The track is the first entry where
  `Flags_Test(Cond_Flags + entry[1] * 8, entry[2])` is set. If none is set, the
  track is the byte after the list.
- Row 20 in `entry[1]` would be the event bank. The table is in the exe's data.
  Its rows were not read for this file. Called by `Area_PickMusic`
  (`window_task.cpp:207`, call at `:213`).

### 5. Movement-script variable 12: `MoveScript_Variable`

- `move_cmds.cpp:449`: variable 12 is `Long(Cond_Flags + 0xA0)`, the event bank's
  first dword (flags 0..31). `kStoryFlags` `0x904030` in
  `move_cmds_callees.h:32` names the same dword. Variable 2 is the current
  chapter row's first dword (`move_cmds.cpp:440-442`).
- `MoveScript_Flow` ops `04..09` (`move_script.cpp:145-159`):
  `have = variable(p[2])`. `want` is `p[1]` for even ops and `variable(p[1])` for odd ops.
  - Ops `04` / `05` test `have == want`. Ops `06` / `07` test `have < want`. Ops
    `08` / `09` test `(want & have) != 0`. The result picks one of two labels.
  - With a byte constant as the mask (op `08`), only flags 0..7 can be masked.
    The compare ops see the 32 flags as one number. Reading, not checked.
- `MoveScript_GroupF` ops `F0` / `F1` (`move_script.cpp:227-231`) call
  `ScriptFlags_Clear40` / `ScriptFlags_Set40` (table at `move_script.cpp:64`).
  They do not touch the event bank.

### 6. Object triggers: `Field_ObjectTriggerByKind`

- `field_e2.cpp:1064-1067`: the handler for an object's byte `+0x86` (an index
  into the table at `WorldMap_FieldHooks + 0x2C`) is called with
  `(object, Cond_Flags + 0xA0)`. The data picks the handler, and the handler is
  given the event bank.
- The handlers (`Field_ObjectTriggers`, 65 entries, `symbols.toml:50456`) are in
  area and rest files. `area_w0c.cpp:563` notes that one of them ignores both
  arguments. What the others do with the bank is not read here.

### 7. New Game seed: `Scenario_SubInit`

- `title_states.cpp:440-451`: the 16 bytes of `Scenario_SubInitTable`
  `0x662CD8` are copied over `Cond_Flags + 0xA0 .. +0xAF`. This is a whole write of
  event flags 0..127, not an OR. It also sets `SaveMenu_NewGameByte`
  (`0x9039F2`, its `s[2]`) to 1.
- It is entry 0 of `Field_ModeTail`'s two-entry table, chosen while
  `SaveMenu_NewGameByte` is 0 (`field_modes.cpp:121-124`). `TitleFlow_NewGame`
  sets that byte to 0 and then calls `Scenario_Start(0)` (`save_menu.cpp:1025-1027`).
  `Scenario_Start` ends with `Field_ModeDispatch` (`title_states.cpp:423`), and
  `Field_ModeDispatch` ends with `Field_ModeTail` (`field_modes.cpp:115-117`).
- The byte is at block `+0x12`, so a loaded game holds 1 and the seed does not
  run again. Reading, not checked.

## Sites

`var` rows marked "loop" have literal bounds in code, not a number from data.

| Flag | Op | Function | File:line | Context |
|---|---|---|---|---|
| 119 | test | `Area_ReturnGate` | `event_ops.cpp:1393` | A step onto a `0xA1` cell (input flags `0x40`, `Cond_ByteFD` 1, not area `0xBB`) does nothing unless the flag is set. |
| 119 | clear | `Area_ReturnGate` | `event_ops.cpp:1395` | Cleared, then `Field_ChangeArea` to the return point `AreaW2d_ReturnPoint`. |
| 28 | clear | `EffectKind04_HoldTick` | `field_c1.cpp:267` | Cleared when effect kind 4's countdown `+9` reaches 0. `Effect_HoldFlag1C` (`scena_sx2.cpp:97`) sets it and spawns this effect. |
| 54 | set | `EffectKind30_Slide` | `field_c2.cpp:204` | In area `0x54`, set when the kind-`0x30` object stops at exactly (`0x118000`, `0x4A8000`). |
| 54 | clear | `EffectKind30_Slide` | `field_c2.cpp:206` | In area `0x54`, cleared when it stops anywhere else. |
| 68 | set | `EffectKind30_Shatter` | `field_c2.cpp:228` | In area `0x92`, set when the kind-`0x30` object's shatter count reaches 0. |
| var | test | `Field_AfterBattleTally` | `field_e2.cpp:210` | Loop over 246..255 (`0xF6..0xFF`) for the first clear flag. Only from chapter 8 with `FieldE2_LevelA` not 0. |
| var | set | `Field_AfterBattleTally` | `field_e2.cpp:228` | That first clear flag (246..255) is set when the counter `Rest4a_ClockA` reaches its threshold. The threshold depends on records counted at `Community_Records`. |
| 245 | test | `Field_AfterBattleTally` | `field_e2.cpp:238` | Tested after each return from battle. |
| 245 | set | `Field_AfterBattleTally` | `field_e2.cpp:239` | Set when clear and `Rest4a_LevelLit` is 4 or more. |
| 30 | clear | `FieldTail_DropInMove` | `field_e2.cpp:1212` | State 4, when counter byte `0x90384B` is `0x31`, after the area change. |
| 31 | clear | `FieldTail_DropInMove` | `field_e2.cpp:1213` | The same step as flag 30. |
| var | test | `FieldTail_StoryWarp` | `field_e2.cpp:1416` | Loop from 255 down to 246 for the highest set flag. It picks the area `0xB9 - (255 - n)`, or `0xAF` when none is set, for `Field_ChangeArea`. |
| 119 | test | `Field_PartySetUp` | `field_event.cpp:555` | With input flag `0x40`, outside areas `0xBD` and `0xBB`, a set flag places the whole party, not the leader alone. |
| 119 | test | `Rest_Begin` | `field_s.cpp:238` | When set: state 6 and script message `0xEF`, nothing more. |
| 119 | set | `Rest_Begin` | `field_s.cpp:245` | Otherwise set, unless the chapter is `0xE` and its row's flag 5 is set. Then `Transition_Start(0)`. |
| 119 | clear | `Rest_PlaceParty` | `field_s.cpp:285` | Cleared when the area is `0x85` (place kind 1). |
| 119 | test | `PartyForm_Reload` | `field_s.cpp:548` | With `AreaW3c_CampFlag` set: a clear flag, or area `0xBB`, outside area `0x85`, reloads the leader alone. |
| var | test | `ItemUse_FaerieTiara` | `item_use.cpp:270` | Loop from 255 down to 246. `ModeRest_AreaAsked` = `0xB9` less one for each clear flag above the first set one. Keeps the return point. |
| 28 | clear | `Area_Enter` | `mode_flow.cpp:560` | Cleared on every area entry, after the placement script and the area hook. |
| 119 | clear | `GameMode3_Leave` | `mode_rest.cpp:187` | `at::kTripFlag` (`mode_rest_callees.h:45`) is `0x77`. Cleared on the branch that clears `AreaW3c_CampFlag` and changes area to the kept return point. |
| var | test | `EventCond_StoryFlag` | `field_o.cpp:1304` | Event-script condition 12. The flag is the script's operand byte. |
| var | test | `EventCond_Flag` | `event_script.cpp:707` | Event-script condition 2, on row `EventScript_FlagBank`. The event bank after op `F9 14`. |
| var | test | `Area_TestCondition` | `map_cells.cpp:93` | Map-data code kind `0x14`: the event flag in the code's low byte. |
| var | test | `Area_TestCondition` | `map_cells.cpp:94` | Map-data code kind `0x34`: the same flag, inverted. |
| var | test | `Area_ZoneTrack` | `window_task.cpp:222` | A music-set entry with row 20 tests event flag `entry[2]`. |
| var | test | `MoveScript_Variable` | `move_cmds.cpp:449` | Variable 12 is the dword of flags 0..31, compared or masked by `MoveScript_Flow` ops `04..09`. |
| var | set | `Scenario_SubInit` | `title_states.cpp:450` | Flags 0..127 overwritten from the 16-byte table `0x662CD8` on the first scenario frame of a New Game. |

17 rows name a literal flag. There are 7 distinct literal flags: 28, 30, 31, 54,
68, 119, 245. Literal loops cover 246..255.

## What each flag means here

In code terms. "Saves" refers to [`../observed-rows.md`](../observed-rows.md)
(22 sections; save 21 is marked probably crafted there).

- **28** (`0x1C`): a short hold flag. `Effect_HoldFlag1C` sets it for `0xF`
  frames (`symbols.toml:61976`: a switch's cool-down for area 49's cell hook). It
  is cleared by `EffectKind04_HoldTick` at the end and by every `Area_Enter`. Not
  set in any save.
- **30, 31** (`0x1E`, `0x1F`): cleared together at the end of
  `FieldTail_DropInMove`. Their setter is not in the engine files. Not set in any save.
- **54** (`0x36`): in area `0x54`, follows whether a pushed kind-`0x30` object rests
  at one position (a state flag, it can change back). Set in saves 20..22.
- **68** (`0x44`): set once a kind-`0x30` object shatters in area `0x92`. Set in
  saves 20..22.
- **119** (`0x77`): "away from the kept return point". `Rest_Begin` sets it.
  `Area_ReturnGate`, `Rest_PlaceParty` (area `0x85`) and `GameMode3_Leave` clear
  it. `Field_PartySetUp` and `PartyForm_Reload` test it to choose the whole party
  or the leader alone. Set in saves 9, 18 and 19.
- **245** (`0xF5`): set after a battle once `Rest4a_LevelLit` is 4 or more. Set in
  saves 20..22.
- **246..255** (`0xF6..0xFF`): a sequence, set one at a time in order by
  `Field_AfterBattleTally` from chapter 8. The highest one set picks the
  destination area for `FieldTail_StoryWarp` and `ItemUse_FaerieTiara`. All set in
  saves 20..22.
- **0..127 at New Game**: whatever `Scenario_SubInitTable` holds. Saves 1..6 hold
  exactly 3, 25 and 27 at the start of the game.
- **146** (`0x92`), for reference: no engine site. Set by `Scena17_EndReturn`
  and read by `Save_BuildBlock` (`rest_2c.cpp:402`). Not set in any save.

## Save, load and New Game

- **Save.** `Save_BuildBlock` (`rest_2c.cpp:372`, not an engine file) copies the
  whole live block `0x9039E0`, `0x10B0` bytes, to the staging buffer
  (`rest_2c.cpp:405-409`). The event bank goes with it at `+0x650`, unchanged.
  It also stores `Flags_Test(event bank, 0x92)` in the slot summary byte
  `0x904694` (`rest_2c.cpp:402-403`). `FieldSave_Write` (`rest_2c.cpp:225`) and
  the quick save (`rest_2c.cpp:423`) both call it.
- **Load.** `LoadMenu_Read` (`save_menu.cpp:1126`) copies the file's `0x10B0`
  bytes over the live block `kGameBlock` `0x9039E0` (`save_menu_callees.h:55`)
  while summing them (`save_menu.cpp:1139-1143`). The event bank is restored
  whole. The copy happens before the checksum compare, so a bad file has already
  overwritten it (comment `save_menu.cpp:1124-1125`). `LoadMenu_Apply`
  (`save_menu.cpp:1213`) does not touch the bank.
- **New Game.** `TitleFlow_NewGame` (`save_menu.cpp:1023-1031`) zeroes the first
  dword of each 8-byte row from `Cond_Flags` up to `kCondFlagsEnd` `0x904030`
  (`save_menu_callees.h:74`). It stops at the event bank and does not clear it.
  The event flags are set by `Scenario_SubInit` on the first scenario frame
  (mechanism 7): flags 0..127 are overwritten from `0x662CD8`, and flags 128..319
  are not written.
- **Flags 3, 25 and 27 one frame into a new game** come from that copy. Reading,
  not checked: the table's bytes were not read (rule: no values from `BOF3.exe`).
  No engine code names flags 3, 25 or 27.

## Unresolved

- **Do the shipped data use the generic paths on the event bank?** That would be
  event-script condition 12, `F9 14` before condition 2, map codes of kind
  `0x14` / `0x34`, music-set rows of 20, and movement-script variable 12.
  Settling it means a scan of the data (for example `tools/event_scan.py`
  conditions), which this read did not do. `event-script.md` section 1 lists
  conditions 0, 2 and 8 as the ones the scripts reach.
- **Flags 128..319 at New Game.** Nothing in the engine clears them. After a
  game, a return to the title and a New Game, they could keep the old values.
  Reading, not checked. The same question applies to the title demo
  (`Title_StateStartDemo`, `title_states.cpp:191`), which calls `Scenario_Start(0x10)`
  without setting `SaveMenu_NewGameByte`.
- **The object-trigger handlers** that receive the event bank
  (`Field_ObjectTriggerByKind`) are in other groups' files.
- **The setter of flags 30 and 31**, and any engine setter of the 0..127 flags
  beyond the seed, are outside the engine files.
- **Event flags 256..319** can be reached only through rows `0x15..0x18` of the
  row-based paths, or by a direct access. No engine site was found that does so.
