# Story flags: the tables in the exe that hold flag numbers

**Status:** IN PROGRESS (2026-11-09, layouts read from source; no table value is quoted here)

Many sites in [`event-flags.md`](event-flags.md) ("Sites whose flag number comes
from the game data") and in the chapter maps take a flag number from a table in
`BOF3.exe`'s `.data`, not from a literal. [`recipes/flag-tables.tsv`](../../recipes/flag-tables.tsv)
gives the layout of each such table, one line for each flag field, so that
`tools/flag_tables.py` can read the numbers from the player's own exe. This repo
does not quote them (CLAUDE.md rule 1). Each column comes from the code that
indexes the table. **address** is the named symbol's `pc` in `symbols.toml`, or
the literal in the file's `*_callees.h`. **stride** and **offset** come from the
`base + i * stride + offset` arithmetic. **count** comes from the loop bound, the
table's end address, or the bounds check our code adds in front of the read.
**bank** is the flag pointer passed to `Flags_Test` / `Flags_Set` / `Flags_Clear` /
`Flags_Toggle`: `Cond_Flags + 0xA0` is `event`, `Cond_Flags + 0x48` is `chapter 9`,
and `Chapter_FlagRow` is `current`. **adjust** is any constant the code adds
before it uses the byte. `read` means that the indexing was followed in the code.
`likely` means it was followed, but one part is not certain (the doubt is given
below). None of the code reads treats a value as "no flag", so no line has an
empty-entry marker. Note that `flag_tables.py` prints a byte of 0xFF as `-`.

Two values are new to the vocabulary of the brief. Kind `row` marks a byte that is
a `Cond_Flags` row number, not a flag number. Bank `by-row` marks a flag whose row
is held in another field.

## The tables

| Recipe line | Address | Layout | Evidence |
|---|---|---|---|
| Area41_TriggerFlags | `0x5F5F68` | 2 x 1, event | `Area41_TriggerFlag` (`area_w1a.cpp:724`) tests `0x5F5F2D` + the object's `+0x86` (`kArea41TriggerBase`, `area_w1a_callees.h:104`). Only trigger ids 59 and 60 call it, so the bytes read are `0x5F5F68` and `0x5F5F69`. **likely**: the code does not show that `+0x86` holds the trigger id. That comes from the comment and the `symbols.toml` entry. |
| Area44_Switches | `0x5F7082` | 4 x 5, +3, event | `Area44_SwitchHook` (`area_w1b.cpp:625`) steps `rec += 5` up to `kA44SwitchesEnd` `0x5F7096` and toggles `At(rec)[3]` on `Bank()` = `Cond_Flags + 0xA0`. |
| Area49_Zones | `0x5FBB70` | 7 x 6, +5, event | `Area49_EffectFrame` (`area_w1c.cpp:462`, `:471`) reads `row[5]`, row = zone * 6. The zone comes from `Area49_MemberZone`, which loops `kArea49ZoneCount` = 7. |
| Area49_CellSwitches | `0x5FBC44` | 9 x 4, +3, event | `Area49_CellHook` (`area_w1c.cpp:525`) loops `kArea49CellSwitchCount` = 9 and toggles the byte at `+ i * 4 + 3`. |
| Area52_CellSwitches | `0x5FE0F0` | 4 x 5, +3, event | `Area52_CellHook` (`area_w1c.cpp:895`) loops 4 records (`i * 5u`) and toggles `r[3]`. |
| Area77_CellSwitches | `0x60BBCC` | 2 x 4, +3, event | `Area77_CellHook` (`area_w2a.cpp:156`) loops `kArea77CellSwitchCount` = 2 and toggles the byte at `+ i * 4 + 3`. |
| Area86_Switches | `0x611C7C` | 3 x 4, +3, event | `Area86_SwitchHook` (`area_w2b.cpp:272`) steps by 4 up to `kA86SwitchesEnd` `0x611C88` (`k == 3` is "none") and toggles `+ 3 + k * 4`. |
| Area108_HookCells | `0x61E63C` | 4 x 4, +3, event | `Area108_CellHook` (`area_w2f.cpp:438`) loops `kArea108HookCellCount` = 4 (`i * 4`) and toggles `e[3]` on `StoryFlags()` = `Cond_Flags + 0xA0`. |
| Area112_HookCells | `0x61FF80` | 4 x 4, +3, event | `Area112_CellHook` (`area_w2f.cpp:1135`, `:1151`) tests and then sets `e[3]`. The loop bound is `kArea112HookCellCount` = 4. Byte 2 is a bit mask into the next table, not a flag number. |
| Area112_HookFlags | `0x61FFA0` | 4 x 1, event | The same hook (`area_w2f.cpp:1145`) reads `Area112_HookFlags + (bit & 3)` for each set bit of `e[2]`: bits 0..3 clear, bits 4..7 set. |
| Area117_Rects / Area118_Rects | `0x6215A8` / `0x621DF0` | 1 x 6, +5, event | `MembersFrame` (`area_w3a.cpp:957`, `:967`) tests `At(t.rects + 4 + found * 6 + 1)`. That is byte 5 of a 6-byte record. `kTw117` / `kTw118` (`area_w3a_callees.h:122`, `:124`) bound the table at `+6`, which is one record. |
| Area117_Switches / Area118_Switches | `0x62160C` / `0x621E3C` | 1 x 4, +3, event | `SwitchHook` (`area_w3a.cpp:1010`) toggles `t.switches + index * 4 + 3`. `switch_count` is 1, and the end is `+4`. |
| Area139_CellEntries | `0x62EDBC` | 2 x 4, +3, event | `Area139_CellHook` (`area_w3e.cpp:332`) uses `FindCell(..., kArea139CellCount = 2, 4, ...)` and toggles `+ i * 4 + 3`. |
| Area140_CellEntries | `0x62EFF8` | 8 x 6, +3, event | `Area140_CellHook` (`area_w3e.cpp:531`, `:549`) uses `kArea140CellCount` = 8 and `kArea140CellStride` = 6, and tests and sets `e[3]`. The flag is copied to the effect's `+0xB`, and `Area140_Effect71End` (`:600`) clears it there. |
| Area109_SwitchFlags | `0x65DE60` | 3 x 1, event | `Area109_SwitchPattern` (`rest_3g.cpp:598`) and `Area109_SwitchHook` (`:615`, `:616`) loop `i < 3` over `kPatternFlags + i` (`rest_3g_callees.h:54`). The table has no symbol name, so the name is ours. |
| Area167_Switch | `0x63C594` | 1 x 4, +3, event | `Area167_CellHook` (`area_w4a.cpp:1205`) steps by 4 up to `kA167SwitchEnd` `0x63C598` and toggles `kA167Switch + 3 + found * 4`. That is the byte at `0x63C597`. |
| EffectKind18_06_Flags | `0x65DB84` | 3 x 1, event | `EffectKind18_06_Start` (`effect_5a.cpp:573`) stores `kFlags6[+0x36]` into `+0x34`, with `Index` bounding it to `kSets6` = 3. `_Wait` (`:584`) tests `+0x34` on `Cond_Flags + 0xA0`, and `_Turn` (`:603`) clears it. |
| EffectKind18_07_FlagRows | `0x65DDB4` | 3 x 1, row | `DoorFlag` (`effect_5a.cpp:168`) tests `0x903F90 + 8 * kFlagRows7[k]`, with k the door `+0xB` (`kDoors7` = 3, `Door()`). A row byte of 0..19 is a chapter row. A row byte of 20..24 is the event bank. |
| EffectKind18_07_FlagBits | `0x65DDB8` | 3 x 1, by-row | The same test: the flag is `kFlagBits7[k]` within that row. For a row of 20 and above, the event flag number is the bit + 64 * (row - 20), as the adjust column says. |
| EffectKind18Sub0C_Flags | `0x65DFC4` | 8 x 1, event | `Sub0CFlag` (`effect_5b.cpp:567`) reads `Byte(kSub0CFlags, kSub0CFlagCount = 8, +0x36)`. `_Start` / `_Wait` / `_Idle` (`:581`, `:592`, `:609`) test it on `Flags()` = `Cond_Flags + 0xA0`. |
| EffectKind18Sub13_Flags | `0x65E010` | 4 x 1, chapter 9 | `Sub13` (`effect_5b.cpp:779`) is `Byte(array, kSub13Count = 4, +0xB)`. It is tested on `Row9()` = `Cond_Flags + 0x48` at `:799`, `:813`, `:839`. |
| EffectKind18Sub13_Faces | `0x65E014` | 4 x 1, chapter 9 | The same `Sub13` read, tested on `Row9()` at `effect_5b.cpp:817`, `:831`, `:857`. |
| EffectKind18_GateFlags | `0x65E034` | 4 x 1, chapter 9 | Sub-kind 0x0F's start (`effect_5b.cpp:972`) stores `Byte(kGateFlags, kGateFlagCount = 4, +0x3A)` into `+0xB`, which is tested on `Row9()` at `:999` and `:1067`. Sub-kind 0x50 reads the same bytes by `+0xB` as `kSub50Lights` (`effect_5c.cpp:302`, `:320`, `:352`, `Sub50Byte` bound 4) on `Cond_Flags + 0x48`. |
| EffectKind18Sub50_Flags | `0x65E094` | 4 x 1, chapter 9 | `EffectKind18Sub50_Start` / `_Shine` / `_WaitSet` (`effect_5c.cpp:278`, `:296`, `:331`) read `Sub50Byte(EffectKind18Sub50_Flags, +0xB)` and test it on `Cond_Flags + 0x48`. |
| EffectKind18Sub14_Flags | `0x65E31C` | 2 x 1, chapter 9 | `EffectKind18Sub14_WaitFlag` (`effect_5d.cpp:1072`) reads `kSub14Flags + Place(kSub14Count = 2)` by `+0x36` and tests it on `Cond_Flags + 0x48`. The comment in `effect_5d_callees.h:54` says "2 bytes (of 4)". |
| EffectKind18Sub22_Flags | `0x65E348` | 4 x 1, event | `EffectKind18Sub22_Start` (`effect_5d.cpp:1390`) stores `kSub22Flags + Place(kSub22Count = 4)` into `+0x34`. `Sub22Flag` (`:1372`) tests `+0x34` on `Cond_Flags + 0xA0`. |
| EffectKind18Sub42_Places | `0x65E8E0` | 3 x 4, +3, event | `EffectKind18Sub42_Start` (`effect_5f.cpp:501`) sets `+0xB = At(place + 3)`, with `place = base + 4 * i` and i bounded by `kSub42PlaceCount` = 3. `_WaitFlag` (`:518`) tests `+0xB` on `Cond_Flags + 0xA0`. |
| EffectKind18Sub5C_Flags | `0x65F38C` | 2 x 1, event | `EffectKind18Sub5C_Place` (`effect_6d.cpp:270`) sets `+8 = kSub5CFlags[v]`, with v the variant `+0x36` bounded by `kSub5CVariants` = 2, and sets the flag on `Cond_Flags + 0xA0`. `_WaitNear` (`:292`) clears it. |
| Scena10_Pickups | `0x661528` | 16 x 6, +0, current, +32 | `Scena10_Object05` (`scena_sc9b.cpp:2164`) sets `B(rec) + 0x20`, and `Scena10_PickupPose` (`:2234`) tests it, with `rec = base + 6 * k` and k bounded by `kPickupCount` = 16 (`PickupAt`, `:378`). The bank is `Bits()` = `Chapter_FlagRow` (`:97`). That this row is chapter 10's is the reading in [`chapter-10.md`](chapter-10.md). |
| Scena15_Battles | `0x6618C0` | 10 x 10, +8, current | `Scena15_Run2` case 5 (`scena_sc15.cpp:620`) sets `kBattles + 8 + idx * 10` on `Row()` = `Chapter_FlagRow` (`:199`). **likely**: idx is the effect's `+0xB`, which the code does not check. The count of 10 is the table's extent in `symbols.toml` (100 bytes) and the comment at `:146`. |
| Scenario_SubInitTable | `0x662CD8` | 16 bytes, bits, event | `Scenario_SubInit` (`title_states.cpp:450`) copies four dwords from the table over `Cond_Flags + 0xA0 .. +0xAF`, which are event flags 0..127. It is a bit image, not a list of numbers. |
| Area_MusicSets | `0x669A48` | 12 x 8, **unknown** | `Area_ZoneTrack` (`window_task.cpp:222`) tests `Cond_Flags + entry[1] * 8`, bit `entry[2]`. See Unresolved. |

## Unresolved

- **Area_MusicSets** (`0x669A48`, `kMusicSets`, `window_task_callees.h:58`). Each
  8-byte set holds a pointer at `+0` to a list of 3-byte entries (track, row,
  flag) and a count at `+4`. The flag is entry byte 2, in the row that entry byte
  1 gives (row 20 is the event bank). The count of 12 sets comes from
  `window_task_fuzz.cpp:54` (`kSets`). The recipe cannot express a table behind a
  pointer, so the line has `?` for offset and adjust, and `flag_tables.py` skips
  it. To read it, a tool would have to follow each pointer, step through `+4`
  entries of 3 bytes, and take bytes 1 and 2.
- **Area41_TriggerFlags** is `likely` only. The original indexes
  `0x5F5F2D + object +0x86` with no bound. The two-entry table assumes that
  `+0x86` is the `Field_ObjectTriggers` id and that only ids 59 and 60 reach
  this handler.
- **Scena15_Battles** is `likely` only. The index is not checked, so the count is
  the table's extent, not a bound in the code. The bank is the current row. That
  this row is chapter 15's is a reading.
- **Sites that are not exe tables** are not in the recipe:
  - `Area42_ClearMemberFlag` (`area_w1b.cpp:135`) uses the active member's byte
    `+0xA0` + 0x10. That is a run-time object, not a table.
  - `Area_TestCondition` codes (`map_cells.cpp:93`, `:94`), event-script
    conditions 2 and 12 (`event_script.cpp:707`, `field_o.cpp:1304`) and the
    object `+0x86` values of the `Scena*_Objects` handlers come from the area and
    script data files.
  - `MoveScript_Variable` 12 (`move_cmds.cpp:449`) reads a whole dword of
    flags 0..31.
  - Sub-kind 0x51 (`effect_6c.cpp:914`) and the 246..255 loops in `field_e2.cpp`
    and `item_use.cpp` use literals.
- **Sub-kind 7's banks.** Whether any `EffectKind18_07_FlagRows` byte is 20 or more
  (the event bank) or below (a chapter row) is in the values. The recipe gives
  the rule. It does not give the answer.
