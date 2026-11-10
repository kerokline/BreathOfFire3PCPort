# Save block range E: `+0xCA0..+0x10AF` (`0x904680..0x904A8F`), the end of the block

**Status:** READ (2026-11-10), from the reimplemented source in this worktree
and its docs. No executable, save file or game data was opened. Block offset =
address - `0x9039E0`. Tags: **READ** (the code that writes or uses the byte was
read), **CITED** (someone's note, not checked in code), **UNKNOWN** (no access
found in the source).

Two facts frame the whole range:

- The block is written and read **whole**. `Save_BuildBlock`
  (`src/game/rest_2c.cpp:372-414`) copies all `0x10B0` bytes to the staging
  copy and sums them; a load copies a file back over the block. So every byte
  here persists through a save and a load whether or not any code names it.
  The field-by-field writers below are the ones that give a byte its value.
- Most of the range (`+0xCD0..+0x103B`) is the faerie village's state
  ("community", areas 175..185). The code that owns it is
  `src/game/rest_4a.cpp` (the simulation `CommuSim_*`), `rest_4b.cpp` (the
  board, the gift tails), `rest_4d.cpp` / `rest_4e.cpp` (names, item and music
  screens) and `field_e2.cpp` (`Field_AfterBattleTally`). The names
  "food", "culture" and so on are Red Herring's (via
  `docs/gitIgnored/red-herring-notes-review.md` O4 in the main checkout) and
  are given in brackets as CITED names; the mechanism is READ.

Units: every "stamp" and "clock" below counts **battles**, not time: it is
compared with `Game_BattleCount` `0x904134` = block **`+0x754`**
(`docs/rest_4a.md` section 1; `symbols.toml` `Game_BattleCount`).

## 1. Byte map

| Offset | Address | Size | Name / meaning | Writers | Readers | Tag | Conf. |
|---|---|---:|---|---|---|---|---|
| `+0xCA0` | `0x904680` | 5 | Summary name, bytes 0..4 of a 9-byte PC name. `strncpy(.., 5)` from the leader's record (record of `Character_IndexById[party id 0]`, party id 0 = `Cond_Flags[0xd2]` = `0x904062` = block `+0x682`); with DIV-0076 on (ours, default) from record 0. Zero-padded after a NUL | `Save_BuildBlock` `rest_2c.cpp:390,393-394` | `Menu_DrawSaveSlot` `field_o.cpp:631-635` (with the 4 tail bytes, `Text_DrawAt` count 5 characters) | READ | high |
| `+0xCA5` | `0x904685` | 3 | Summary party icons: field order `+0x682..+0x684` (`kPartyList` `0x904062` + `0x623`), id 4 written as `0x0B` unless `AreaW0b_PartySet` (`0x90412C`, block `+0x74C`) & 0x7F is 5 or 0xC (`FormOf`). `0xFF` = none | `Save_BuildBlock` `rest_2c.cpp:396`, `FormOf` `rest_2c.cpp:111-115` | `Menu_DrawSaveSlot` `field_o.cpp:627-630` | READ | high |
| `+0xCA8` | `0x904688` | 1 | Summary level = record 0 `+0x0A` (`0x903A7A`, block `+0x9A`) | `rest_2c.cpp:397` | `Menu_DrawSaveSlot` `field_o.cpp:636,642` (number and EXP bar) | READ | high |
| `+0xCA9` | `0x904689` | 1 | Copy of `Config_WindowColour` `0x903A5A` (block `+0x7A`) | `rest_2c.cpp:399` | none found (no reader of summary `+9`; `records.h:421` `style`) | READ | high |
| `+0xCAA` | `0x90468A` | 1 | Copy of `Config_Background` `0x903A5B` (block `+0x7B`) | `rest_2c.cpp:400` | none found (`records.h:422` `style2`) | READ | high |
| `+0xCAB` | `0x90468B` | 1 | Padding. Not written by `Save_BuildBlock`; carried by the summary's 7-dword copies | none (whole-summary copies only: `CopySummary` `rest_2c.cpp:100-103`, `StagedRead` `save_menu.cpp:337-341`) | none | READ (no meaning) | high |
| `+0xCAC` | `0x90468C` | 4 | Summary play clock: dword copy of `0x9040C8` (block `+0x6E8`: h, m, s, frames) | `rest_2c.cpp:398` | `Menu_DrawSaveSlot` `field_o.cpp:638` (hours `+0xC`, minutes `+0xD` only) | READ | high |
| `+0xCB0` | `0x904690` | 4 | Summary EXP = record 0 `+0x0C` dword (`0x903A7C`, block `+0x9C`) | `rest_2c.cpp:401` | `Menu_DrawSaveSlot` `field_o.cpp:642` (`Menu_DrawExpBar`) | READ | high |
| `+0xCB4` | `0x904694` | 1 | "Cleared" mark: `Flags_Test(Cond_Flags + 0xA0, 0x92)` (event flag 146, block `+0x650`) | `rest_2c.cpp:402-403` | `Menu_DrawSaveSlot` `field_o.cpp:643,648` (frame flags 2 and piece `0x3D`) | READ | high |
| `+0xCB5` | `0x904695` | 1 | In a slot summary in memory: the `Save_Directory` index, `0xFF` = empty slot. In the block nobody writes it, so a save carries whatever the block held (0 from the boot's zeroed block, CITED `symbols.toml` `MountainPasses_Bits` evidence) | in memory: `save_menu::ReadSummaries` `save_menu.cpp:316,328`; in the block: none | `LoadMenu_Choose` `save_menu.cpp:1090`, slots draw `save_menu.cpp:825`, quick menu `quick_menu.cpp:93` (all test `!= 0xFF`) | READ | high |
| `+0xCB6` | `0x904696` | 4 | Summary name, bytes 5..8: `strncpy(.., 4)` from the same record's name `+5` | `rest_2c.cpp:395` | `Menu_DrawSaveSlot` `field_o.cpp:633` (copied after bytes 0..4) | READ | high |
| `+0xCBA` | `0x90469A` | 2 | Padding (`records.h:433` `pad_1A`) | none (whole-summary copies only) | none | READ (no meaning) | high |
| `+0xCBC` | `0x90469C` | 18 | The nine pad words, a copy of `0x903580..0x903591` at save time: word 0 Speak, 1 Move, 2 Menu (`Field_MenuButton`), 3 View, 4 Change, 5 select (no line reads it alone), 6 Action, 7 confirm (`Field_ConfirmButtons`), 8 cancel (`Field_CancelButtons`) (`docs/controls.md` 2.1 table) | `Save_BuildBlock` `rest_2c.cpp:384,385,387,389,391` (dwords `0x903580/84/88/8C`, word `0x903590`) | `LoadMenu_Apply` `save_menu.cpp:1217-1224` (all 18 bytes back to `0x903580`), then the Start-held reset `1225-1236` and DIV-0133 `sigma::LoadApplied` `save_menu.cpp:1240`, `sigma.cpp:464-472` | READ | high |
| `+0xCCE` | `0x9046AE` | 2 | No access found | none | none | UNKNOWN | - |
| `+0xCD0` | `0x9046B0` | 4 | `Rest4a_ClockA`: battles counted toward the next clear story flag of `0xF6..0xFF`; at `max(5, t[n-0xF6] - bias)` the flag is set and this is 0 (t = 20 20 20 40 20 20 60 20 20 20) | `CommuSim_AreaEnter` `rest_4a.cpp:331` (0 at the village's start); `Field_AfterBattleTally` `field_e2.cpp:224,229` | `Field_AfterBattleTally` `field_e2.cpp:224-230` | READ | high |
| `+0xCD4` | `0x9046B4` | 4 | `Rest4a_StampGrow`: battle count at the last population growth | `rest_4a.cpp:332,415,421,425` | `CommuSim_Population` `rest_4a.cpp:409-425` | READ | high |
| `+0xCD8` | `0x9046B8` | 4 | `Rest4a_StampShrink`: battle count at the last population shrink | `rest_4a.cpp:333,407,422,430` | `CommuSim_Population` `rest_4a.cpp:407,422,427` | READ | high |
| `+0xCDC` | `0x9046BC` | 4 | `Rest4a_StampMood`: battle count at the last mood update | `rest_4a.cpp:334,448,453` | `CommuSim_Mood` `rest_4a.cpp:439` | READ | high |
| `+0xCE0` | `0x9046C0` | 4 | `Rest4a_StampLevelLit`: stamp for the lit level. Moved by the cost at each level up; **pushed up by one per battle** while no kind-4 building with `+1` set has a resident (so it pauses the count) | `rest_4a.cpp:335,489`; `Records_CountUnpaired(1, ..)` `field_e2.cpp:162-171,233` | `CommuSim_LevelLit` `rest_4a.cpp:486-487` | READ | high |
| `+0xCE4` | `0x9046C4` | 4 | `Rest4a_StampLevelDark`: the same for the dark level (kind-4 buildings with `+1` clear) | `rest_4a.cpp:336,512`; `Records_CountUnpaired(0, ..)` `field_e2.cpp:234` | `CommuSim_LevelDark` `rest_4a.cpp:508-509` | READ | high |
| `+0xCE8` | `0x9046C8` | 2 | `Rest4a_AreaSeen` u16: the highest area number the village queue has seen; areas past it whose byte `0x652850[n]` is 4 or 5 queue an event | `rest_4a.cpp:342,389` | `CommuSim_QueueAreas` `rest_4a.cpp:365-366` | READ | high |
| `+0xCEA` | `0x9046CA` | 1 | `Rest4b_CountA`, s8 0..99 ("mood"; CITED name "food"). 10 at the start; moved by the trait `+0x10` sum of records at place 9 less 2 x count, per 5 battles; gates growth (> 4 x records) and shrink (0) | `rest_4a.cpp:337,449,452` | `CommuSim_Population` `rest_4a.cpp:400`; `CommuSim_Mood` `446`; board panel `rest_4b.cpp:1285` (printed) | READ | high |
| `+0xCEB` | `0x9046CB` | 1 | `FieldE2_LevelA` u8 below 10, the "dark level" (CITED name "jobs unlocked"): 1 at the start; the board's kind list has this many rows (building kind = row + 4); 0 disables the after-battle tally | `rest_4a.cpp:338,514` | `CommuSim_LevelDark` `rest_4a.cpp:501,510`; `CommuPose_Kind4` `983`; `Field_AfterBattleTally` `field_e2.cpp:206,234`; board lists `rest_4b.cpp:841-847,1572-1614` | READ | high |
| `+0xCEC` | `0x9046CC` | 1 | `Rest4a_LevelLit` u8 0..7, the "lit level" (CITED name "culture"). Gates the dark level (`0x65291B[dark]`), caps kind 5 (`+4`) and kind 0xB (`0x652958[lit]`); at 4 or more sets story flag `0xF5` | `rest_4a.cpp:339,491` | `rest_4a.cpp:479,484,511,546,705,717,983`; `field_e2.cpp:233,238`; panel `rest_4b.cpp:1293` | READ | high |
| `+0xCED` | `0x9046CD` | 1 | `FieldE2_BiasA`: sum of trait `+0x11` over records in use at place `0xA` (CITED "clear power"); lowers the thresholds of flags `0xF6`, `0xF9`, `0xFC` | `CommuSim_SumKindsAB` `rest_4a.cpp:786,793`; `rest_4a.cpp:340` | `Field_AfterBattleTally` `field_e2.cpp:225` | READ | high |
| `+0xCEE` | `0x9046CE` | 1 | `FieldE2_BiasB`: the same over place `0xB` (CITED "build power"); lowers the other seven thresholds | `rest_4a.cpp:341,788,796` | `field_e2.cpp:225` | READ | high |
| `+0xCEF` | `0x9046CF` | 1 | `AreaW4d_HookArmed`: area 185's exit-square arm byte. Cleared by every village area's init, set when the leader is off the square, consumed on it | `Area179_Init` `area_w4d.cpp:242`; `Area185_StepHook` `area_w4d.cpp:402,406` | `Area185_StepHook` `area_w4d.cpp:405` | READ | high |
| `+0xCF0` | `0x9046D0` | 480 | `Community_Records`, 60 x 8 (spec "faerie jobs"); fields in section 1.1 | section 1.1 | section 1.1 | READ | high |
| `+0xED0` | `0x9048B0` | 64 | `Community_Buildings`, 8 x 8 (spec "faerie rooms"), addressed 1..8 as `0x9048A8 + 8 * slot`; fields in section 1.2 | section 1.2 | section 1.2 | READ | high |
| `+0xF10` | `0x9048F0` | 300 | Names, 60 x 5, by record index; no terminator between slots; the release's text encoding | `CommuSim_AddRecord` `rest_4a.cpp:749-751` (trait record `0x653200 + 0x14 r`, bytes `+0..+4`); `CommuName_CommitEntry` `rest_4e.cpp:602-635` (edit buffer `0x675F98`, NUL stored as space) | board card `rest_4b.cpp:1074`; `CommuEntry_DrawPanel` `rest_4e.cpp:212-215`; `CommuDraw_DrawTitle` `rest_4d.cpp:538-541`; `CommuName_SlotEntryIn` `rest_4d.cpp:802-806` (0 read as space); `CommuRank_DrawListC` `rest_4e.cpp:1372-1373`; DIV-0200 `faerie_name.h` | READ | high |
| `+0x103C` | `0x904A1C` | 4 | No access found | none | none | UNKNOWN | - |
| `+0x1040` | `0x904A20` | 1 | `MountainPasses_Bits` (**ours**, DIV-0191): bit 0 = the party has crossed Mt. Levett (area 70) once; under the switch `MountainPasses` only | `MountainPasses_AfterPlacement` `event_script.cpp:393-394` | `event_script.cpp:393,399` | READ | high |
| `+0x1041` | `0x904A21` | 111 | No access found (to `+0x10AF`; `0x904A90` is outside the block: `Rest4b_BusyA`, the spawn count, not saved) | none | none | UNKNOWN | - |

### 1.1 The 60 records at `+0xCF0` (stride 8; record `i` at `+0xCF0 + 8i`)

Record `i` is a fixed individual: its traits are row `i` of `0x653200`
(`0x14` bytes: name `+0..+4`, four stats `+0x10..+0x13`, CITED as PWR, CON,
CUL, INT) and its name slot is `+0xF10 + 5i` (`rest_4a.cpp:56-58,749-751`).

| Field | Meaning | Writers | Readers |
|---|---|---|---|
| `+0` u8 | In use (non-zero). Set 1 on spawn; 0 on removal | `CommuSim_AddRecord` `rest_4a.cpp:745`; `CommuSim_RemoveRecord` `776`; `CommuSim_TickKind9` `592` (a departure) | every walk: `CommuName_CountSlots` `rest_4d.cpp:1185-1190`, `rest_4a.cpp:74,443,465,842`, `field_e2.cpp:168,182,217` |
| `+1` u8 | Place: 0 not placed, 1..8 the building (1-based), 9, `0xA`, `0xB` three loose places. Records at place 9 feed the mood; `0xA` / `0xB` feed BiasA / BiasB and the story-flag tally | `AddRecord` `rest_4a.cpp:746` (0); `CommuBoard_PlaceRecord` `rest_4b.cpp:1705` (the board's pick) | `rest_4a.cpp:74,443,465,538,579,628,708,791,843,927`; `field_e2.cpp:168,182,218-219`; `CommuMusic_Browse` `rest_4e.cpp:712`; `FieldTrigger09` `rest_4a.cpp:1177` |
| `+2` u8 | State, read by the building kind: kind 9 a trip state 0..3 (1 too recent, 2/3 a gift ready, `CommuPose_Kind9`); kind `0xD` the deposited item id (`0x37` after a failure). 0 on spawn and on placement | `AddRecord` `rest_4a.cpp:748`; `TickKind9` `582,597,605`; `TickKindD` `678`; `PlaceRecord` `rest_4b.cpp:1708`; `CommuBoard_Confirm` `rest_4b.cpp:993`; gift tails `rest_4b.cpp:310,341,430`; `CommuItem_Choose` `rest_4e.cpp:1059` | `TickKind9` `rest_4a.cpp:585`; `TickKindD` `631`; `CommuPose_Kind9` `1046`; `CommuTail_RandomGift` `rest_4b.cpp:305`; `CommuTail_NibbleGift` `422,425`; `CommuItem_Confirm` `rest_4e.cpp:1100`; `CommuItem_Give` `1135` |
| `+3` u8 | Kind `0xD` only: high nibble the status (0 none, 1 deposited, 2 / 3 / 4 outcomes), low nibble the item category (0 consumables, 1 weapons, 2 armour, else accessories) | `CommuItem_Choose` `rest_4e.cpp:1063` (the tab); `CommuItem_Give` `1137` (`|= 0x10`); `TickKindD` `rest_4a.cpp:660-680`; `PlaceRecord` `rest_4b.cpp:1709`; `Confirm` `rest_4b.cpp:994`; `NibbleGift` `431` (0) | `TickKindD` `rest_4a.cpp:629-634`; `NibbleGift` `rest_4b.cpp:409,426`; `CommuItem_Confirm` `rest_4e.cpp:1099`; `CommuItem_Give` `1135` |
| `+4` u32 | Battle-count stamp: spawn, placement, gift, deposit | `AddRecord` `rest_4a.cpp:747`; `PlaceRecord` `rest_4b.cpp:1706`; `Confirm` `992`; gift tails `261,309,340`; `CommuItem_Give` `rest_4e.cpp:1138` | `TickKind9` `rest_4a.cpp:581`; `TickKindD` `658-659`; `CommuPose_Kind8` `1037`; `CommuTail_TimedGift` `rest_4b.cpp:243` |

### 1.2 The 8 buildings at `+0xED0` (stride 8; slot `s` = 1..8 at `+0xEC8 + 8s`)

| Field | Meaning | Writers | Readers |
|---|---|---|---|
| `+0` u8 | Kind: 0 empty, else the board's row + 4, so 4..`0xD` (CITED: 4 scholar, 5 merchant, 6 inn, 7 gift, 8 fortune, 9 explorer, A antiques, B music, C casino, D copy) | `CommuBoard_Confirm` `rest_4b.cpp:1001` | every tick `rest_4a.cpp:463,528,575,624,698`; `CommuSim_PlaceResident` `947`; `field_e2.cpp:165,179`; board `rest_4b.cpp:717,1102,1771` |
| `+1` u8 | The first sub-list's row: kind 4 a lit / dark switch (non-zero lit), kind 9 a level 0..2, kind 5 a term in the pose's `+0x1C` | `Confirm` `rest_4b.cpp:1002` | `LevelSums` `rest_4a.cpp:463`; `TickKind9` `580,601`; `CommuPose_Kind4` `981-986`; `CommuPose_Kind5` `1003`; `CommuTail_RandomGift` `rest_4b.cpp:316`; `Records_CountUnpaired` `field_e2.cpp:166` (the key) |
| `+2` u8 | The second sub-list's row (the "toggle"): kind 5 a term of its divisor | `Confirm` `rest_4b.cpp:1005` | `TickKind5` `rest_4a.cpp:543`; `CommuPose_Kind5` `1003` |
| `+3` u8 | Level: kind 5 1..11 (capped at lit + 4); kind `0xB` 5..`0x652958[lit]`, also the music list's track count (`kTrackCounts` `0x9048AB + 8 * slot`); `FieldTrigger09` copies it into a shop record's `+0`; non-zero gates `Records_CountUnpairedOfKind` | `TickKind5` `rest_4a.cpp:530,551-552`; `TickKindB` `701,716-718`; `Confirm` `rest_4b.cpp:997` (0) | `rest_4a.cpp:529,534,548,699`; `FieldTrigger09` `1179`; `CommuMusic_Browse` `rest_4e.cpp:712`; `field_e2.cpp:180` |
| `+4` u32 | Battle-count stamp of the level; pushed up by one per battle while the building has no resident (kinds 5 and `0xB` with `+3` set) | `TickKind5` `rest_4a.cpp:531,553`; `TickKindB` `702,719`; `Confirm` `rest_4b.cpp:998`; `Records_CountUnpairedOfKind` `field_e2.cpp:183` | `TickKind5` `rest_4a.cpp:545`; `TickKindB` `713` |

No reader or writer of a building's `+5..+7` exists apart from the dword at
`+4` (searched `Bld(..)[5..7]`, `Slot(..) + 5..7`).

## 2. The current spec rows (`docs/save-editor-spec.md` 7)

| Spec row | Verdict |
|---|---|
| `+0xCA0` 28 Slot summary, D | **Confirmed** as 28 bytes, derived. |
| summary `+0xCA0` 5 Name, "record 0's name" | **Corrected.** The PC name is 9 bytes in the summary: bytes 0..4 at `+0xCA0` **and bytes 5..8 at `+0xCB6`** (`rest_2c.cpp:394-395`; drawn together, `field_o.cpp:631-633`). `docs/save-interchange.md` 2's "the summary's 5-byte name did not widen" is wrong for the same reason. The source record is the **field leader's** in Capcom's code; record 0's only with DIV-0076 on (`rest_2c.cpp:367-370,393`). The 2 of 75 misses are likely saves made by the original with another leader (not checked). |
| summary `+0xCA5` 3 Portraits | **Confirmed**, rule READ: field order, id 4 becomes `0x0B` unless `+0x74C & 0x7F` is 5 or `0xC`. |
| summary `+0xCA8` 1 Level | **Confirmed** (record 0 `+0xA`). |
| (no row) `+0xCA9`, `+0xCAA`, `+0xCAB` | **New:** copies of `+0x7A` window colour and `+0x7B` background; `+0xCAB` padding. Spec 12.1's "`+0x7B`, `+0xCAA` probably the background and its copy" is now READ. |
| summary `+0xCAC` 4 Play clock | **Confirmed**: dword copy of `+0x6E8`. |
| summary `+0xCB0` 4 EXP | **Confirmed** (record 0 `+0xC`). |
| summary `+0xCB4` 1 Cleared | **Confirmed**. |
| (no row) `+0xCB5`, `+0xCB6..+0xCB9`, `+0xCBA..+0xCBB` | **New:** directory-index cell (never `0xFF`), name bytes 5..8, padding. |
| `+0xCBC` 18 Nine button masks, CITED | **Confirmed READ**: a save-time copy of `0x903580..0x903591` and restored at load. The boot's 18-byte default table goes to `0x903580`, never to `+0xCBC` directly. |
| `+0xCCE` 2 Unnamed | **Still unknown**: no access. |
| `+0xCD0` 26 counters at `+0xCD0`, `+0xCDC`, `+0xCE0` and unnamed bytes | **Corrected:** no unnamed bytes. Seven fields: `+0xCD0` tally (u32), `+0xCD4` grow, `+0xCD8` shrink, `+0xCDC` mood, `+0xCE0` lit, `+0xCE4` dark stamps (u32), `+0xCE8` area seen (u16). The ones called "counters" are battle-count stamps except `+0xCD0`. `Field_AfterBattleTally`'s "tally" names in `field_e2_callees.h:33-35` are these same cells. |
| `+0xCEA` 5 food, max jobs, culture, clear, build power | **Confirmed** as five bytes in that order (mood / dark level / lit level / BiasA / BiasB), names CITED. Correction: clear and build power are **derived** sums, rebuilt on each village entry that finds records (`rest_4a.cpp:355,785-799`). |
| `+0xCEF` 1 Unnamed | **Corrected:** `AreaW4d_HookArmed`, transient. |
| `+0xCF0` 480 jobs 60 x 8 | **Confirmed**; fields in 1.1. "Jobs" are the 60 individuals (records), not job types. |
| `+0xED0` 64 rooms 8 x 8 | **Confirmed READ**; fields in 1.2. (frost's "9 x 8" counts the slot-0 base `0x9048A8`, which is record 59; the code writes only slots 1..8, `rest_4b.cpp:104-109`.) |
| `+0xF10` 300 names 60 x 5 | **Confirmed.** |
| `+0x103C` 116 Tail, 1 byte live | **Confirmed** as no Capcom access. One byte is ours: `+0x1040` (DIV-0191). See open question 1 on which byte the "1 live" measurement saw. |
| spec 12.1 lines 443-445 (`+0xCAD..+0xCAF` clock copy, six button bytes from `+0xCBC`, `+0xCAA`) | **Confirmed** by the writers above. |

## 3. What an editor may edit, and what must agree

Derived, regenerate on write (Advanced may override):

- `+0xCA0..+0xCBB` from: name record (record 0, or the leader `+0x682[0]`
  for a faithful original), `+0x682..+0x684` with the id-4 rule on `+0x74C`,
  record 0 `+0xA` and `+0xC`, `+0x6E8`, event flag 146, `+0x7A`, `+0x7B`.
  Keep `+0xCB5` not `0xFF` (write 0): after a save the summary is copied
  into the slot table as-is (`rest_2c.cpp:413`) and `0xFF` there marks the
  slot empty (`save_menu.cpp:1090`). `+0xCAB`, `+0xCBA..+0xCBB`: preserve.
- `+0xCED`, `+0xCEE`: sum of trait `+0x11` over records in use at place `0xA`
  / `0xB` (`rest_4a.cpp:785-799`), byte arithmetic. The game rewrites them on
  the next village entry anyway.

Editable with constraints:

- **Buttons `+0xCBC`.** With DIV-0133 `ButtonSigma` on (default), a load
  rewrites word 7 = word 0 and word 8 = word 6, then requires words 0, 1, 6,
  2, 3, 4 (word 0 without its L2/R2 bits `0x03`) to be each exactly one of
  the six bits `0xFC` and all different, **or the game aborts**
  (`sigma.cpp:94-103,392-417`; `sigma.h:65-66`). Keep them a permutation; set
  `0x03` on word 0 and word 7 as the defaults do (`docs/controls.md` 2.1).
- **Every battle-count stamp must not exceed the battle count `+0x754`:**
  `+0xCD4..+0xCE7` (five dwords), each record's `+4`, each building's `+4`.
  All are compared as `count - stamp` unsigned. A stamp above the count makes
  the difference huge: the lit / dark levels rise at once and keep rising
  each entry (`rest_4a.cpp:487-489,509-512`), kind 5 / `0xB` levels jump to
  their caps, gifts give the top row. An editor that changes `+0x754` should
  move every stamp by the same amount.
- `+0xCEA` 0..99 (clamped by the game, `rest_4a.cpp:447-452`). `+0xCEC` 0..7,
  `+0xCEB` 1..10 once the village has started (0 = not started: the tally
  is off, `field_e2.cpp:206`). `+0xCD0` any value (compared `>=`; the next
  tally sets the flag).
- Building kind `+0` 0 or 4..(`+0xCEB` + 3); any other non-zero kind makes
  `CommuBoard_SlotHelp` read past its table (ours aborts, `docs/rest_4b.md`
  5). Kind 9 needs `+1` <= 2 (`rest_4a.cpp:602-604`, `rest_4b.cpp:316-320`:
  ours aborts, the original reads its stack frame). A kind-`0xB` building
  with 6 residents divides by zero (`rest_4a.cpp:710-712`). More than three
  residents in one building read the next building's places
  (`rest_4a.cpp:917-925`).
- Records: `+1` in 0..`0xB`; at most 59 in use while a spawn can run
  (all 60 in use: the original's spawn loops forever, `rest_4a.cpp:739`); at
  least 1 in use once started (0 restarts the village: three spawns and every
  stamp and level reset, `rest_4a.cpp:327-344`). The game itself grows to 20.
  `+2` / `+3` per the building kind (1.1); a record at kind `0xD` with
  `+3 = 0x1n` holds an item already taken from the inventory.
- Names: free text in the destination release's encoding, 5 bytes, space or
  NUL padded. Names are tied to the record index, persist when a record is
  freed, and are overwritten from the trait table when that record spawns
  again (`rest_4a.cpp:749-751`). Do not end a slot on half of a two-byte
  glyph: Capcom's draw then reads into the next slot and can hit the
  `in al, dx` trap (D401; DIV-0200 fixes it in ours, `faerie_name.h`).
- `+0xCEF`: write 0 (cleared by every village area's init anyway).
- `+0x1040`: bit 0 only, meaningful only to our build with `MountainPasses`.
  The original ignores it. Preserve the rest of the tail.

Preserve: `+0xCCE..+0xCCF`, `+0x103C..+0x103F`, `+0x1041..+0x10AF`.

## 4. Open questions and possible defects

1. **Which tail byte is "live"?** The spec's MEASURED "1 byte live" and the
   older note that `+0x103C` is "the last non-zero byte of the US save"
   (`save-interchange.md` 2a) do not name the same thing for certain. The
   name table is `+0xF10..+0x103B` (`0x12C` bytes), so `+0x103C` itself is
   outside it, and no code reads or writes it. Candidates: `+0x1040` set by
   DIV-0191 in the owner's PC saves made since 2026-10-30, or a byte from the
   PlayStation side in the US save. Settle it with the measurement script
   (offset and which samples), not by opening saves here.
2. `+0xCCE..+0xCCF`: no access. Possibly alignment before the `+0xCD0` dword.
3. The PlayStation side (sibling `tools/save_tool.py:66-67`) shows the same
   summary at `+0xCA0` and the nine words at `+0xCBC` copied from one buffer
   `0x80145AB0`; its summary `+9` / `+0xA` are unlabelled there. On the PC
   they are the two style bytes. Whether the PSX writes `+0xCB5..+0xCBB`
   was not read.
4. Defect (Capcom's, faithful in ours): the summary takes the **leader's**
   name but **record 0's** level and EXP (`rest_2c.cpp:390-401`); DIV-0076
   fixes the name to record 0.
5. Defect (Capcom's): the summary's `+0xCB5` is never written in the block
   but becomes the slot's "present" flag when the summary is copied after a
   save. Harmless while the block's byte is 0; a block with `0xFF` there
   saves a slot that shows empty until the directory is read again.
6. Defects already ledgered or described, relevant to an editor:
   `CommuBoard_Confirm` restarts records that are **not in use** but last sat
   in the slot (`docs/rest_4b.md` 5, faithful); spawn / remove walk forever
   with 60 / 0 records (ours aborts); the gift tails and `PlaceRecord` can
   write record 255 (`0x904EC8`, past the block) in the original.
7. Not checked here: whether the PC load path validates the checksum
   (outside this range; `save-interchange.md` 2).

## Summary

1,040 bytes. **READ 923** (summary 28, buttons 18, village scalars 32,
records 480, buildings 64, names 300, `+0x1040` 1), **CITED 0** (Red
Herring's names are given beside READ mechanisms), **UNKNOWN 117**
(`+0xCCE..+0xCCF`, `+0x103C..+0x103F`, `+0x1041..+0x10AF`).
