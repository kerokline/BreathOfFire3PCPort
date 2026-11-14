# Save block range C: `+0x670..+0x773` (party, counters, bit sets)

**Status:** READ 2026-11-10, one reading pass over the source in this worktree
(read only; no exe, no saves). Addresses are `0x904050..0x904153`; block offset =
address - `0x9039E0`. Most code reaches this range as `Cond_Flags[0xC0..0x1C3]` or
`Cond_Flags_addr + N` (`Cond_Flags` = `0x903F90`). The chest-flag bank
`+0x6EC..+0x70B` (`0x9040CC..0x9040EB`) is settled and is only bounded here.

Method: `analysis/data_census.tsv` filtered to the range, then a grep of the
source for every literal address `0x904050..0x904153`, for `Cond_Flags[0xC0..0x1C3]`
and `Cond_Flags_addr + 0xC0..0x1C3`, and for the named symbols
(`symbols.toml` `[[data]]` entries at `pc = 0x904058`, `0x9040EC`, `0x90409C`,
`0x9040C9`, `0x9040CB`, `0x90412C..0x904153`, `0x904131`, `0x904101`). Each writer
and reader below was opened and read.

Tags: **READ** (code that writes or uses it was read), **CITED** (a note, not
checked in code), **UNKNOWN** (no code found).

## 1. The byte map

| Offset | Address | Size | Meaning | Writers | Readers | Tag | Conf. |
|---|---|---:|---|---|---|---|---|
| `+0x670` | `0x904050` | 8 | **No access found.** The 8 bytes after event flag 255 and before the zenny. Not reached through the event bank: `Flags_Set/Clear/Test` mask the flag number to 8 bits (`event_script.cpp:680-696`), so the bank `0x904030` ends at `+0x66F`. Not reached by any literal, `Cond_Flags[0xC0..0xC7]` or symbol. Reachable only in principle by data: `Area_TestCondition` row `kind & 0x1F` with an 8-bit bit number (`map_cells.cpp:93-94`) reaches `Cond_Flags + 0xF8 + 31` = `+0x6C7`, and `EventCond_Flag` takes any row by `EventScript_FlagBank & 0xFF` (`event_script.cpp:706-708`) | none | none in code | UNKNOWN | high that no code touches it |
| `+0x678` | `0x904058` | 4 | Party zenny, u32, capped at 9,999,999 | `Zenny_Add` (`scena_sx.cpp:525-534`, also adds to `+0x758`), `Zenny_Sub` (`scena_sx.cpp:513-518`), `Area61` choice sets 0 (`area_w1d.cpp:497`), area 175..185 choices (`area_w4d.cpp:102`) | money boxes, shops (`symbols.toml` `Party_Zenny` module list) | READ | high |
| `+0x67C` | `0x90405C` | 2 | Area 189's walk **frame word**, u16: +1 each step; at `0x1E0` and `0x3C0` it fires messages 5 / 6 and at `0x3C0` returns to 0 | `Area189_StepArrive` (`area_w4e.cpp:599-618`); set 0 by `Scena14_Run1` (`scena_sc13.cpp:1651`, `:1908`, a word store), `Scena15_Run1` (`scena_sc15.cpp:533`), `Area150_Tail58` (`area_w3g.cpp:866`), `Area191_Tail53` and `Area191_Kind18Flag77` (`area_w4e.cpp:968`, `:1102`), `Area193_Tail57` (`area_w4f.cpp:523`); set `0x1E0` by `ResetWord1E0` (`area_w4f.cpp:156-160`) and `Area192_Tail54` (`area_w4f.cpp:259`) | `Area189_StepArrive`; `EffectKind18Sub44_Draw` fades by it (`effect_6c.cpp:590`, below `0x1B0`, `0x1E0`) | READ | high |
| `+0x67E` | `0x90405E` | 1 | Area 189's **walk count** (`kWalkCount`): +1 each time the frame word reaches `0x3C0` and at two scripted arrivals; picks which talk message members give | +1: `Area189_StepArrive` (`area_w4e.cpp:615-619`), `Area191_Tail53` state 4 (`area_w4e.cpp:966-971`), `Area191_Kind18Flag77` (`area_w4e.cpp:1101-1106`); 0: `Area150_Tail58` (`area_w3g.cpp:868`), `Area193_Tail57` (`area_w4f.cpp:525`) | `Area191_TalkMessage` (`area_w4e.cpp:1046`, capped at 8), `Area191_Init` (`area_w4e.cpp:1083-1085`, `>= 5`), `Area192_TalkMessage` (`area_w4f.cpp:334`), `Area192_Init` (`area_w4f.cpp:381-386`) | READ | high |
| `+0x67F` | `0x90405F` | 1 | Area 189's **reserve**: counts down 3 (or 1) per step, + 8 at the frame word's two marks; at 0 message 1 and the walk halts | `Area189_StepArrive` (`area_w4e.cpp:603-633`); `0xF0` by `ItemUse_WaterJug` (`item_use.cpp:282-285`, the source's name), `Area150_Tail58` (`area_w3g.cpp:867`), `Area193_Tail57` (`area_w4f.cpp:524`) | `Area189_StepArrive` | READ | high |
| `+0x680` | `0x904060` | 1 | **Active formation**: the cell index `2 * row + col - 1` of the tactics grid, 0..5 | `TacticsFormation_Pick` (`rest_2f.cpp:552`, `:599`); `Party_Remove` clears it when the party shrinks (`scena_sx.cpp:271-275`: 0 when going to 1 member, 0 if `>= 3` when going to 2) | `TacticsFormation_Build` (`rest_2f.cpp:857-860`); `Party_PlaceInFormation` (`scena_sx2.cpp:121`); damage: value 2 takes a quarter off damage to the party (`battle_damage.cpp:551`, `:848`); value 5 makes every member's HP change start at -1 (`battle_setup.cpp:513`); battle sprites (`battle_sprites.cpp:348`), run weights (`battle_e2.cpp:752`, `:759`), encounter placing (`inventory_ops.cpp:427`) | READ | high |
| `+0x681` | `0x904061` | 1 | Unlocked formation cells. **Settled** (BRIEF); seen here: `Scena16_PartyReset` ORs bits 0..2 (`move_cmds.cpp:494-498`) | settled | settled | READ | settled |
| `+0x682` | `0x904062` | 3 | **Field order** (party list 0): character ids, `0xFF` padding at the end. `Party_Count(0)` = entries before the first `0xFF`, at most 3 (`field_event.cpp:285-290`) | `Party_Join` (`field_event.cpp:727`, also writes list 1); `Party_Remove` (`scena_sx.cpp:250-262`); `Party_DropIn` reorders (`area_entry.cpp:158`); `Field_LeaderStepLands` (`event_leader.cpp:464`); swaps in the party menus (`field_s.cpp:773`, `rest_2f.cpp:971`); `Scena16_PartyReset` 6 x `0xFF` (`move_cmds.cpp:497`) | about 40 sites: `Save_BuildBlock` (summary name record and portraits, `rest_2c.cpp:390`, `:396`), `GameMode3_Leave` and `PartyForm_Reload` (`PartySet_Load` of the three, `mode_rest.cpp:151`, `field_s.cpp:545`), `Party_HealJoined` (`scena_sx.cpp:219`) | READ | high |
| `+0x685` | `0x904065` | 3 | **Battle order** (party list 1): the same ids in battle-row order | `Party_Join` (`field_event.cpp:728`); `Party_Remove` (`scena_sx.cpp:242-248`); the formation grid's row swap (`rest_2f.cpp:972`, `field_s.cpp:774`); scenes: `SortParty` sets `(0, 0xFF, 0xFF)` and keeps the old bytes outside the block (`scena_sc6.cpp:1158-1161`), `scena_sc7.cpp:2277-2278` restores two bytes kept outside the block | `Party_PlaceForBattle` (`scena_sx.cpp:166`), battle draw and equip (`battle_draw.cpp:298`, `battle_e7.cpp:862`, `battle_e4.cpp:150`, `:1202`), menus (`menu_lists.cpp:185`, `:413`), `inventory_ops.cpp:433`, `:776` | READ | high |
| `+0x688` | `0x904068` | 32 | **Enemy kinds beaten**: 256 bits, bit = enemy kind (u8, `records.h:269`). Copied to an enemy's `+0x8F` at set-up; the HP gauge and the identify panel show by that copy (`cheats.cpp` DIV-0187 notes) | `Battle_EnemyDefeated` (`battle_flow.cpp:347`) through `Battle_SetEnemyKindFlag` (`battle_sprites.cpp:1028-1031`) | `Battle_EnemyKindFlag` (`battle_sprites.cpp:1022-1025`) from `Battle_SetupEnemy` (`battle_sprites.cpp:794`) | READ | high |
| `+0x6A8` | `0x904088` | 32 | **Abilities learnt**: 256 bits by the ability id's low byte. Set when a party member learns an enemy's ability in battle, and by a master's grant | `Battle_SettleFlag8` (`battle_sprites.cpp:311`, flag bit 3 path); `MasterGrant_Step1Member` (`rest_2d.cpp:493-500`) | `Battle_ActionBitSet` (`battle_e6.cpp:917-920`) for `Battle_MemberRollByAction` and the settle step; `MasterGrant_Step1Member` (skip a granted one); `Area74_ChoiceAsk92` tests bit `0x20` of `0x90409C` = **bit `0xA5` of this set** (`area_w1f.cpp:365-376`); DIV `UnlearnedBlue` (`battle_actions.cpp:160-170`) | READ | high |
| `+0x6C8` | `0x9040A8` | 32 | **Enemy kinds identified**: 256 bits by the target enemy's kind (`+0x8C`) | `Identify_MarkSeen` (`magic_s12.cpp:386-390`) | `Identify_WasSeen` (`magic_s12.cpp:393-396`) | READ | high |
| `+0x6E8` | `0x9040C8` | 4 | **Play clock**: hours, minutes, seconds, frames (30 a second). Stops at 99:59:59 | `Game_ClockTick` every field-task frame unless `Field_Request` 9 (`mode_flow.cpp:464-490`); `Title_StateStartDemo` zeroes it (`title_states.cpp:199-202`) | `Save_BuildBlock` copies it to the summary `+0xCAC` (`rest_2c.cpp:398`); `GameMode_Start`: **all four zero means a new game**, otherwise the saved track is restored (`mode_flow.cpp:347-356`); the save-slot clock (`map_field_objects.cpp:557-561`); `FieldTrigger02` prints h, m (`rest_4a.cpp:1141-1143`) | READ | high |
| `+0x6EC` | `0x9040CC` | 32 | Chest / field-object content flags: 256 bits by the content object's `+5`, which op `9x` of the area's placement script gives (`op[8]`; place and contents in [`story-flags.md`](../story-flags.md) "Other flag banks"). `0xFF` = no flag | `Field_ContentTake` (`field_4.cpp:1640`) | `Field_ContentTake`; `EventOp_9x` at placement (`event_op.cpp:193`: set, the object is drawn taken or not placed) | READ | high |
| `+0x70C` | `0x9040EC` | 32 | **Fish record sizes**, one byte per fish kind, 0 = never caught (`symbols.toml` `Fish_RecordSizes`, "23 used of 32") | `LeaderPanel_S4Best` (`effect_1e.cpp:687-697`): a larger catch's size replaces it | fishing panels `PanelKind`, `PanelRow` (`effect_1a.cpp:1715-1735`, `effect_1f.cpp:431`); world-map spot signs: 0 shows `????????` (`area_w0b.cpp:136`, `area_w1b.cpp:911`, `area_w1e.cpp:151`, `area_w2b.cpp:388`, `area_w3a.cpp:166`, `area_w3g.cpp:978`); `Shisu_ScaleIndex` reads entry `0x15` (`0x904101`, `rest_2b.cpp:752`) | READ | high |
| `+0x72C` | `0x90410C` | 32 | **Passage find flags**: a second find bank, flag number from the passage entry's byte `+4` (`0xFF` = no flag) | `Field_PassageTake` (`event_leader.cpp:738-765`) | `Field_PassageTake`; `Scena01_Scene06` step 0 tests flag 4 of it (`scena_sc1.cpp:508`) | READ | high |
| `+0x74C` | `0x90412C` | 1 | **Party set**: the row (0..18) of the party-combination table for the field order's three ids; bit 7 = recorded by a mode-0 load; `0xFF` = none | `PartySet_Select` (`field_event.cpp:826-829`); `GameMode3_Leave` ORs `0x80` (`mode_rest.cpp:157-160`); `Title_StateMenu` (demo) `0xFF` (symbols, title_states) | `PartySet_Find` (`field_event.cpp:761`); **`LoadMenu_Apply` loads the set's files by it** (`save_menu.cpp:1243-1245`); per-set action tables (`event_leader.cpp:782`, `rest_1a.cpp:270`); `Kaiser_ReloadParty` (`magic_s30.cpp:557`); scenario selectors (`scena_sc12.cpp:864`, `scena_sc9a.cpp:261`); world map set `0xC` (`world_map.cpp:198`) | READ | high |
| `+0x74D` | `0x90412D` | 1 | **Copy of `Field_ScriptFlags`'s low byte** (`0x9039A2`, outside the block) | `Save_BuildBlock` (`rest_2c.cpp:404`) at save | `LoadMenu_Read` sets `Field_ScriptFlags` = this byte, zero-extended (`save_menu.cpp:1145`) | READ | high |
| `+0x74E` | `0x90412E` | 1 | **Fishing lure slot**: a category-3 item of kind `0xA` held outside the inventory | `ExtraSlots_PickItem` (`effect_1f.cpp:204`, the kind-`0xA` path); `LeaderPanel_UseItem` clears it when the inventory has no more (`effect_1e.cpp:1004-1008`) | `Inventory_Count` adds `+0x74F` when it is the item (`char_stats.cpp:562-563`); `EffectKind03_ShowName` (`effect_1a.cpp:861`); record pointer `0x66A528 + 20 * (id - 0x1C)` (`rest_1g.cpp:546-547`); "the lure's" (`rest_1g.cpp:89`) | READ | high |
| `+0x74F` | `0x90412F` | 1 | Count held in the lure slot (1 when set) | `ExtraSlots_PickItem` (= 1); `LeaderPanel_UseItem` (= 0) | `Inventory_Count` (`char_stats.cpp:563`) | READ | high |
| `+0x750` | `0x904130` | 1 | **Fishing rod slot**: a category-3 item of kind `0xB` held outside the inventory | `ExtraSlots_PickItem` (`effect_1f.cpp:220-224`: old one back with `Inventory_Add`, new one out with `Inventory_Remove`) | `Inventory_Count` counts it once (`char_stats.cpp:562`); `EffectKind03_ShowName` (reads a dword at `0x904130`, keeps the low byte, `effect_1a.cpp:862`); `effect_1b.cpp:1135-1148`; record `0x66A4E8 + 10 * (id - 0x2E)` (`rest_1g.cpp:547`); "the rod's" (`rest_1g.cpp:88`) | READ | high |
| `+0x751` | `0x904131` | 1 | **Music track playing** (`Music_Track`), `0xFF` none. Saved as it is; restored at the next area entry after a load | `Music_Play` stores it, `Sound_Tick` sets `0xFF` (symbols); many mode sites (`field_s.cpp:282`, `field_modes.cpp:308`, boss scenes); `GameMode_Start` sets `0xFF` (`mode_flow.cpp:356`) | `GameMode_Start`: clock not zero -> `AreaW1f_MusicCurrent` = this (DIV-0092 keeps the area's track when it is `0xFF`) (`mode_flow.cpp:347-355`), then `Area_Enter` plays it (`mode_flow.cpp:520-522`) | READ | high |
| `+0x752` | `0x904132` | 2 | **No access found**, except as the upper bytes of `EffectKind03_ShowName`'s dword load at `0x904130`, which it masks away (`effect_1a.cpp:862`) | none | none | UNKNOWN | high that no code uses them |
| `+0x754` | `0x904134` | 4 | **Battle count**, u32 (`Game_BattleCount`) | +1 when `Field_Request` 3 starts a battle (`mode_tasks.cpp:296-305`), the one writer (symbols) | community record ages `Game_BattleCount - stamp` (`rest_4a.cpp:1034-1037`, `rest_4b.cpp:228`, stamps written by `rest_4e.cpp:1131`); `EffectKind65_Check` acts on a multiple of 5 (`effect_3b.cpp:461-470`); `FieldTrigger02` prints it (`rest_4a.cpp:1144`) | READ | high |
| `+0x758` | `0x904138` | 4 | **Zenny tally**, u32, uncapped: every `Zenny_Add` whose second argument's byte is 0 | `Zenny_Add` (`scena_sx.cpp:528-529`) | `FieldTrigger02` prints it (`rest_4a.cpp:1145`) | READ | high |
| `+0x75C` | `0x90413C` | 4 | **Passage finds taken**, u32 | `Field_PassageTake` (`event_leader.cpp:745`, `:755`) | `FieldTrigger03` (`rest_4a.cpp:1154`) | READ | high |
| `+0x760` | `0x904140` | 4 | **Field-object contents taken** (chests), u32 | `Field_ContentTake` (`field_e1.cpp:513`, `:522`) | `FieldTrigger03` (`rest_4a.cpp:1155`) | READ | high |
| `+0x764` | `0x904144` | 4 | **Fishing sessions entered**, u32 | `GameMode8_Enter` +1 (`rest_3g.cpp:407-412`) | `FieldTrigger03` (`rest_4a.cpp:1156`) | READ | high |
| `+0x768` | `0x904148` | 4 | **Return point x**, 16.16 | `Area189_ExitButton` (`area_w4e.cpp:752`); `GameMode3_Leave` camp path (`mode_rest.cpp` comment `:131-134`); `Field_LeaderCellEvent` gateway exit (`event_ops.cpp:1243-1250`); `ItemUse_FaerieTiara` (`item_use.cpp:276-277`); `Area98_TailChangeArea62` (`area_w2d.cpp:397-399`) | `Area_ReturnGate` (`event_ops.cpp:1386-1402`); `Field_LeaderStepLands` (`event_leader.cpp:250-255`); `ChangeToReturnPoint` (`area_w4d.cpp:149`); `Area191_Tail53` (`area_w4e.cpp:979`); `Area192_Tail54` (`area_w4f.cpp:267-269`) | READ | high |
| `+0x76C` | `0x90414C` | 4 | Return point z, 16.16 | same as above | same | READ | high |
| `+0x770` | `0x904150` | 2 | Return point area, u16 | same as above | same | READ | high |
| `+0x772` | `0x904152` | 1 | **Away from the return point**: non-zero while the party is out on a trip (the source's `kTripOut`, `kCampFlag`) | +1: `Area189_ExitButton` (`area_w4e.cpp:754-756`), the gateway exit (`event_ops.cpp:1245-1250`); 1: camp out (`mode_rest.cpp:131-134`); 0: `Area_ReturnGate` (`event_ops.cpp:1397`), `Field_LeaderStepLands` (`event_leader.cpp:255`), `Area191_Tail53` (`area_w4e.cpp:981`, `:997`), `Area131_TailLeave34` (`area_w3c.cpp:502`), `Area187_TailMessage2` (`area_w4d.cpp:707`), `Area192_Tail54` (`area_w4f.cpp:272`, `:287`), `Scena14_Run1` (`scena_sc13.cpp:1704`) | `FieldMenu_TopBar` allows Camp (`menu_lists.cpp:235-243`); `PartyForm_Reload` places one member when set (`field_s.cpp:546-550`); `GameMode3_Leave` picks out or home (`mode_rest.cpp:131-136`) | READ | high |
| `+0x773` | `0x904153` | 1 | **Arrival facing** for area 189, 0..15 (the leader's facing jittered by -1, 0, +1, 0) | `JitterFacing` for `Area189_ExitButton` (`area_w4e.cpp:126-130`), camp out (`mode_rest.cpp:133`); 2 by `Scena14_Run1` (`scena_sc13.cpp:1649`, `:1906`), `Scena15_Run1` (`scena_sc15.cpp:531`), `Area193_Tail57` (`area_w4f.cpp:522`); `0xA` by `Area150_Tail58` (`area_w3g.cpp:865`) | `Area189_LeaderStart` takes it as the facing and the camera angle (`area_w4e.cpp:397-404`, `docs/area_w4e.md` state 0) | READ | high |

Area numbers and story meaning of area 189's walk are as the source and
[`../area_w4e.md`](../area_w4e.md) give them; what the walk is in the game is not
stated here.

## 2. The spec table's rows (save-editor-spec.md section 7)

| Spec row | Verdict |
|---|---|
| `+0x650` 40 bytes "event flags" | **Corrected for this range:** the event bank is 32 bytes. `+0x670..+0x677` are not event flags and no code reaches them (row 1 above). The row should say 32 bytes, with an 8-byte UNKNOWN row after it |
| `+0x66E` faerie village stage | Outside this range; not checked here |
| `+0x678` zenny | **Confirmed**, READ (u32, cap 9,999,999) |
| `+0x67C` 4 unnamed, 3 live | **Corrected:** area 189's walk state - frame word u16 `+0x67C`, walk count `+0x67E`, reserve `+0x67F` |
| `+0x680` active formation | **Confirmed**, READ: cell index 0..5 (`2 * row + col - 1`) |
| `+0x681` | Settled; not redone |
| `+0x682` field order | **Confirmed**, READ |
| `+0x685` battle order | **Confirmed**, READ |
| `+0x688` 96 unnamed | **Corrected:** three 256-bit sets: enemy kinds beaten `+0x688`, abilities learnt `+0x6A8`, enemy kinds identified `+0x6C8` |
| `+0x6E8` play clock | **Confirmed**, READ; and all-zero has a meaning (new game) |
| `+0x6EC` 32 unnamed | Settled elsewhere as the chest bank; the spec row is out of date |
| `+0x70C` fishing best catch | **Confirmed**, READ: largest size per fish kind, 0 = none |
| `+0x72C` 32 unnamed | **Corrected:** passage find flags (`Field_PassageTake`) |
| `+0x74C` loaded character index | **Corrected:** the party set (combination row, bit 7, `0xFF` none). It names a combination of three, not one character |
| `+0x74D` 4 unnamed | **Corrected:** `+0x74D` saved `Field_ScriptFlags` low byte; `+0x74E` lure id; `+0x74F` lure count; `+0x750` rod id |
| `+0x751` BGM id | **Confirmed**, READ: the track playing when saved, restored after a load |
| `+0x752` 2, measured zero | Confirmed no access (UNKNOWN) |
| `+0x754` battle count | **Confirmed**, READ |
| `+0x758` 28 "lifetime zenny and unnamed" | **Corrected:** `+0x758` zenny tally (only `Zenny_Add` calls with tally byte 0, not every gain), `+0x75C` passages taken, `+0x760` contents taken, `+0x764` fishing sessions, `+0x768` return point (x, z, area), `+0x772` away flag, `+0x773` arrival facing |

## 3. For a save editor

**Safe to edit freely** (nothing else in the block checks them):

- Zenny `+0x678`: keep at most 9,999,999 (`Zenny_Add` clamps only on the next add).
- The three tally counters `+0x758`, `+0x75C`, `+0x760`, `+0x764`: only printed by
  `FieldTrigger02/03`. No code compares them with the find banks.
- The three bit sets `+0x688`, `+0x6A8`, `+0x6C8`. Setting a bit of `+0x6A8` marks an
  ability as learnt for the battle roll and the masters; it does not add the
  ability to a character's list (`AbilityList_Add` is a separate call). Bit `0xA5`
  of `+0x6A8` is also read by area 74's choice.
- Fish records `+0x70C`: one byte per kind; 0 = not caught.
- Passage flags `+0x72C`.

**Must agree with something else:**

- **Party lists `+0x682` and `+0x685`**: the same set of ids, `0xFF` only at the end,
  the same count, at least one member. On load `Field_MemberCount` =
  `Party_Count(0)`, the field list only (`save_menu.cpp:1242`). Each id needs its
  character record's joined bit (`+0xB` bit 0), which `Party_Join` sets.
  The slot summary's portraits `+0xCA5` are made from `+0x682` at save time.
- **Party set `+0x74C`**: the combination row for the field list's ids, as
  `PartySet_Find` would choose it from the table at `0x669750` (in the exe; read it
  from the install). `LoadMenu_Apply` loads the set's files from this byte without
  recomputing it (`save_menu.cpp:1243-1245`), so a byte that does not match the
  party loads the wrong sprite files. Write `row | 0x80` (the state a field save
  leaves after `GameMode3_Leave` or a mode-0 select).
- **Formation `+0x680`**: a cell whose bit is set in `+0x681`, and below the cell
  limit for the member count (1 member: cell 0; 2: cells 0..2; 3: 0..5,
  `TacticsFormation_Build`, `rest_2f.cpp:815-826`). Build does not check it.
- **Clock `+0x6E8`**: equals the summary's `+0xCAC`. Hours at most 99, minutes and
  seconds below 60, frames below 30. **Never all four zero** in a save that is
  meant to resume: `GameMode_Start` reads zero as a new game and does not restore
  `+0x751`.
- **Music `+0x751`**: a track id or `0xFF`. With `0xFF`, Capcom's rule starts the
  area silent; DIV-0092 keeps the area's own track instead.
- **`+0x74D`**: restored into `Field_ScriptFlags` on load. Keep the value the game
  wrote (it is normally the field's idle state); do not invent bits.
- **Battle count `+0x754`**: the community records' stamps (`+4` of the records at
  block `+0xCF0`, and the record at block `+0x15`, `rest_4b.cpp:228`) are compared
  with it by an unsigned difference. Lowering the count below a stamp makes that
  age huge. Raise it, or move the stamps with it.
- **Fishing slots `+0x74E..+0x750`**: items held outside the inventory lists;
  `Inventory_Count` adds them back. `+0x74E` must be a category-3 id of kind `0xA`
  with `+0x74F` 1 (or both 0); `+0x750` a category-3 id of kind `0xB` or 0.
- **Return trip `+0x768..+0x773`**: when `+0x772` is non-zero the game expects a
  real return point (area and position) and sends the party there through the
  return gate or the camp. Write 0 to `+0x772` for a save not on a trip; then the
  other 10 bytes are not read until a trip sets them.
- **Area 189 walk `+0x67C..+0x67F`**: only meaningful inside that sequence; keep
  them as saved.

Preserve byte for byte: `+0x670..+0x677`, `+0x752..+0x753`.

## 4. Open questions and possible defects

1. **New game does not clear this range.** `TitleFlow_NewGame` clears only the
   first dword of each 8 bytes of `Cond_Flags` up to `0x904030`
   (`save_menu.cpp:1023-1028`), and `NewGame_InitCharacters` writes only the
   records (`rest_3a.cpp:828-844`). No code read clears zenny, the counters, the
   bit sets, the fish records or the return trip at a new game. On a first boot
   they are zero because the block is in the image's uninitialised part. If a
   player can start a new game after a game in the same session (back to the
   title), the earlier game's values would carry over. Not checked: whether
   chapter 0's `Scenario_CallA(0)` (`scena_sc0.cpp:131`) resets any of them, and
   whether the title can be reached from play.
2. **`Party_Remove` with an id that is not in the party** (`scena_sx.cpp:233-262`,
   "unchecked" in its comment): with 3 members the searches end at index 3, so it
   writes `0xFF` to `0x904068` (`+0x688`, the first 8 enemy-kind bits all set) and
   to `0x904065` (battle order's first byte). Capcom's; reachable only if a caller
   passes an absent id.
3. **Fish record compare** (`LeaderPanel_S4Best`, `effect_1e.cpp:692-695`): the
   catch's size word is compared with the stored byte and only its low byte is
   stored. A size above 255 would store a wrapped value. Whether sizes reach 256
   was not read.
4. **Flag `0xFF` in both find banks**: `Field_ContentTake` and `Field_PassageTake`
   test the entry's flag before checking for `0xFF`, so a no-flag entry tests the
   bank's last bit (`+0x70B` bit 7, `+0x74B` bit 7). Nothing sets those bits for a
   `0xFF` entry, so the test stays clear; an editor that sets either bit would make
   every no-flag object or passage say "already taken". The zenny path of
   `Field_ContentTake` counts `+0x760` on every take, flag or not
   (`field_e1.cpp:509-513`), so repeatable zenny objects inflate the counter.
5. **"Story flag `0x365`"** (`area_w1f.cpp:363`, `area_w1f_callees.h:50`): flag
   numbers above 255 cannot exist in the event bank (8-bit mask). The byte
   `0x90409C` is bit `0xA5` of the ability set `+0x6A8`; the source comment should
   be relabelled. What ability `0xA5` is was not read.
6. **Data scripts and map codes** can in principle test any bit of `+0x670..+0x6C7`
   (`Area_TestCondition` rows 24..31) or beyond (`EventCond_Flag` after `F9`).
   Whether the game data does so cannot be decided from the source; the flag-table
   recipe (`recipes/flag-tables.tsv`) does not cover map condition codes.
7. **`+0x772` is a counter used as a flag**: two writers add 1, the others store 1
   or 0, and every reader tests non-zero. No reader cares about values above 1.
