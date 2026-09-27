# World 0, areas 16 and 18..26: the world map's second copy and nine small areas

**Status:** IN PROGRESS (2026-09-27) - 61 functions ours
(`src/game/area_w0b.cpp`, shadow name `area_w0b`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)), one `Run` per area:
0 mismatches in 256,000 rounds; controls in section 13. Fuzz only: no
recorded route enters any of the ten areas (section 12). No divergence.

Group AR0B of round ten's second wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) section 6): the band
`0x401B80..0x403400`, whole areas as `tools/area_rows.py --groups` cut them.
What each area *is* in the story is not read here: the names come from what
the code does.

## 1. The band, the areas, the function count

`analysis/area_funcs.tsv` (group `AR0B`) lists 61 starts, none ours. All 61
are functions and all are taken: no start dropped, none added. Read against
the disassembly (capstone, the scratch `adis.py`, every function to its last
instruction):

| Area | Descriptor | Roots | Block | Fns |
|--:|---|---|---|--:|
| 16 | `0x5E5DE8` (only a choice table, `+0x34` -> `0x41D270`, outside the band) | `WorldMap_Records` record 0 (`+0`, `+4`, `+8`, `+0xC`, `+0x10`), `WorldMap_FieldHooks` entry 0, six state tables in its data block | `0x401B80..0x402C92` | 25 |
| 18 | `0x5E6688` | init, `Area18_Handlers` (2) | `0x402CA0..0x402D5E` | 3 |
| 19 | `0x5E6B80` | `Area19_Handlers` (8; three ours) | `0x402D60..0x402DA8` | 3 |
| 20 | `0x5E6C58` | init | `0x402DB0..0x402E7A` | 1 |
| 21 | `0x5E7280` | `Area21_Choices` (5) | `0x402E80..0x402F8E` | 3 |
| 22 | `0x5E7DE8` | `Area22_Choices` (3), `Area22_Handlers` (1) | `0x402F90..0x403077` | 5 (one shared) |
| 23 | `0x5E8768` | `Area23_Choices` (7), `Area23_Handlers` (5) | `0x403080..0x40310F` | 4 |
| 24 | `0x5EA048` | `Area24_Choices` (2) | `0x403110..0x403119` | 1 |
| 25 | `0x5EACD8` | `Area25_Handlers` (1) | `0x403120..0x403176` | 1 |
| 26 | `0x5EC5A0` | `Area26_Choices` (17), `Area26_Handlers` (15) | `0x403180..0x4033F2` | 15 |

Area 17 has no block (its code is shared, [`worldmap_area.md`](worldmap_area.md)).
Every gap in the band is `nop` padding; every jump is internal; no function
has a jump table (the dispatches are `.data` tables, section 2).

**Shared bodies** (keyed by address, taken once):

| PC | Name | Reached by |
|---|---|---|
| `0x401DE0` | `Area16_PlateStart` | `Area16_PlateStates` 0 and `WorldMap33_PlateStates` 0 (areas 16, 33) |
| `0x402D00` | `Area18_ClearCells` | area 18's handler 1, area 19's handler 6 |
| `0x403050` | `Area22_ArmTailOnYes` | nine areas' choice or handler entries: 1, 23, 32, 40, 41, 108, 117 (choice 3 and handler 7), 141, 143 - not area 22 itself; it lies in area 22's block |

**Gap functions** (reached by no descriptor field of their own area, so the
tool marks them): `0x4020B0` (`Area16_FrameStep`, called by
`Area16_HudFrame`), `0x402180` (`Area16_BoxStep`, `Area16_HudFrame`'s tail
jump), `0x4022E0` / `0x4024B0` / `0x402570` (the three draws, called by the
frame and box states). Each is reached by the code named.

**Area 20's init** `0x402DB0` is a five-byte `jmp 0x402DC0` and the body
(the tool's row is the whole `0xCB`): the fuzz clones the body from
`0x402DC0`, as `worldmap_area_fuzz.cpp` did for area 29's.

**The tool's rows against the reading:** every call site the tool printed is
the one read (offset and callee). Two table counts in its notes are run
lengths, not table sizes: `0x5E5F5C` "15 code entries" is five plate states
followed by the HUD, frame and box tables; `0x5E5F70` "10" is two. The
sizes below are the readers' bounds.

## 2. Area 16: the world map's copy

Area 16 is `WorldMap_Records` record 0 (the first of the eleven world-map
areas, [`worldmap_area.md`](worldmap_area.md) section 5). **Its code is area
33's, instruction for instruction, over its own tables** - a capstone
compare of each pair (the scratch `cmp.py`) finds only the jump targets, the
calls to its own copies and the table operands differing:

| Area 16 | Area 33's (owner) | Instructions |
|---|---|--:|
| `Area16_PlateRun` `0x401D00` | `WorldMap33_PlateRun` `0x403E00` (DA) | 65 |
| `Area16_PlateShow` `0x401E30` | `WorldMap33_PlateShow` `0x403EE0` | 87 |
| `Area16_PlateGrow` / `_Hold` / `_Shrink` | `0x404030` / `0x404080` / `0x4040E0` | 17 / 23 / 21 |
| `Area16_HudRun` / `_HudFrame` | `WorldMapHud_Run` / `_Frame` | 4 / 2 |
| `Area16_FrameStep` `0x4020B0` + three states | `WorldMap_FrameStep` `0x404160` (round seven, the states inline there) | 4, 8, 11, 17 |
| `Area16_BoxStep` + `_BoxSlideIn` / `_BoxHold` / `_BoxSlideOut` | `WorldMapHud_Box*` | 4, 28, 35, 23 |
| `Area16_DrawFrame` / `_DrawSprite` / `_DrawHud` | `WorldMap_DrawFrame` / `_DrawSprite` / `_DrawHud` (round seven) | 155 / 56 / 30 |
| `Area16_Record8Run` / `_Record8Place` | `0x404680` / `0x4046A0` (not ours: AR0C's this wave) | 4 / 87 |
| `Area16_Record4Run` / `_Record4MarkCell` | `0x404800` / `0x404820` (AR0C's) | 4 / 46 |
| `Area16_DrawDrift` `0x402830` | `WorldMap33_DrawDrift` `0x4048E0` | 324 |

So the descriptions of [`worldmap_area.md`](worldmap_area.md) sections 2..4
and [`world-map-hud.md`](world-map-hud.md) hold for area 16 with its tables,
and ours is area 33's code over them. Where area 33's frame step carries its
three states inline, area 16's are separate functions reached through
`Area16_FrameStates` (as they are in the exe: `jmp [eax*4 + 0x5E5F78]`
tail-jumps to each).

**The functions only area 16 has, or that this group reads first:**

- **`Area16_PlaceMessage` `0x401B80`** (`WorldMap_FieldHooks` entry 0). Area
  33's hook has one branch; area 16's has two. On state 0 it asks
  `AreaMap_ByteAt` about the leader's cell. A place cell (`0xA1`) opens the
  place's message as area 33's does (nine rows here, not six). Any other cell
  looks the cell up in `Area16_Cells` (138 records of `(x, z, -, id)`,
  searched with **no bound**), finds the id's name set in `Area16_NameSets`
  (three records of an id and four item bytes; none found is set 3), writes
  up to four item names into `Text_Records` rows 0..3 (`"????????"` for an
  item whose byte at `0x9040EC` is 0; the 16-byte name of `Item_NamePtr(0,
  item + 0x38)` otherwise, or of `0x669CD8` for item `0x16`), and opens
  message `set + 0x16`. So the hook shows a list of up to four items for a
  cell, names withheld until seen; what the cells are is not read.
- **`Area16_PlateStart` `0x401DE0`**, the plate's state 0 for both maps:
  `+0x24 = 0x80`, `Sprite_SetAnimationBank(0xF)`, the tint bytes and `+0x2A`
  cleared, `+0x29 = 5`, `+1 = 1`.
- **`Area16_Record8Place` `0x4025F0`** (record `+8`, effect kind `0x16`,
  state 1): bank `0x46`; the object placed at the leader, its direction
  `+0xC` / `+0x10` from `Area16_Directions[+8]`, its cell nudged back by the
  direction (`sar 13`, then `sar 9`) and, unless `+6` is 0, two steps sideways
  (up for `+6 == 1`); an animation and `+0x2A` from `Area16_Record8Anims[+8]`;
  `Sprite_UpdateScreen`.
- **`Area16_Record4MarkCell` `0x402770`** (record `+4`, kind `0xE`, state 0):
  released at once when `0x903A79` is 9 or `Field_StatusBits` bit 0; else
  bank `0x205` and the map byte of the cell `Area16_Cells[+0xB]` set to `0xA0`
  (a place cell by `WorldMap_DrawFrame`'s reading), then state 1 (the
  copies' shared `0x408990`). `WorldMap_RecordIndex` is called and its answer
  never read.

The record's other states (`0x4253C0`, `0x40C490`, `0x408990`) and the HUD's
shared ones (`WorldMap_FrameWait`, `WorldMapHud_BoxWait`, `WorldMapHud_Start`)
are other groups' or ours already; ours reads the tables in place.

**Area 16's tables** (`[[data]]`, in its data block after area 17's
descriptor - the tool moves them to area 16; checked: every table below is
read only by area 16's code): `Area16_PlateAnims` `0x5E5BE0` (9),
`Area16_Cells` `0x5E5C04` (138 x 4), `Area16_PlaceMessages` `0x5E5E2C` (9
rows), `Area16_NameSets` `0x5E5F4C` (3 x 5), `Area16_PlateStates` `0x5E5F5C`
(5), `Area16_HudStates` `0x5E5F70` (2), `Area16_FrameStates` `0x5E5F78` (4),
`Area16_BoxStates` `0x5E5F88` (4), `Area16_Sprites` `0x5E5F98` (22 x 4),
`Area16_Buttons` `0x5E5FF0` (6 x 4), `Area16_Record8States` `0x5E6008` (3),
`Area16_Directions` `0x5E6014` (4 x 2 words), `Area16_Record8Anims`
`0x5E6024` (4 x 2), `Area16_Record4States` `0x5E602C` (2), `Area16_DriftUV`
`0x5E6034` (12). That is 20 code pointers in the state tables, the tool's
count of area 16's moved pointers.

| PC | Name | Size | Root |
|---|---|--:|---|
| `0x401B80` | `Area16_PlaceMessage` | `0x174` | `WorldMap_FieldHooks` 0 |
| `0x401D00` | `Area16_PlateRun` | `0xD6` | record 0 `+0` |
| `0x401DE0` | `Area16_PlateStart` | `0x4E` | `Area16_PlateStates` 0 (shared with 33) |
| `0x401E30` | `Area16_PlateShow` | `0x142` | ... 1 |
| `0x401F80` | `Area16_PlateGrow` | `0x41` | ... 2 |
| `0x401FD0` | `Area16_PlateHold` | `0x58` | ... 3 |
| `0x402030` | `Area16_PlateShrink` | `0x50` | ... 4 |
| `0x402080` | `Area16_HudRun` | `0x12` | record 0 `+0xC` |
| `0x4020A0` | `Area16_HudFrame` | `0xA` | `Area16_HudStates` 1 |
| `0x4020B0` | `Area16_FrameStep` | `0x12` | called by `Area16_HudFrame` |
| `0x4020D0` | `Area16_FrameSlideIn` | `0x21` | `Area16_FrameStates` 1 |
| `0x402100` | `Area16_FrameHold` | `0x28` | ... 2 (and `_FrameSlideIn`'s tail jump) |
| `0x402130` | `Area16_FrameSlideOut` | `0x41` | ... 3 |
| `0x402180` | `Area16_BoxStep` | `0x12` | `Area16_HudFrame`'s tail jump |
| `0x4021A0` | `Area16_BoxSlideIn` | `0x62` | `Area16_BoxStates` 1 |
| `0x402210` | `Area16_BoxHold` | `0x6E` | ... 2 |
| `0x402280` | `Area16_BoxSlideOut` | `0x57` | ... 3 |
| `0x4022E0` | `Area16_DrawFrame` | `0x1C5` | called by the frame states |
| `0x4024B0` | `Area16_DrawSprite` | `0xBC` | called by the two draws |
| `0x402570` | `Area16_DrawHud` | `0x58` | called by the box states |
| `0x4025D0` | `Area16_Record8Run` | `0x12` | record 0 `+8` |
| `0x4025F0` | `Area16_Record8Place` | `0x154` | `Area16_Record8States` 1 |
| `0x402750` | `Area16_Record4Run` | `0x12` | record 0 `+4` |
| `0x402770` | `Area16_Record4MarkCell` | `0xB5` | `Area16_Record4States` 0 |
| `0x402830` | `Area16_DrawDrift` | `0x462` | record 0 `+0x10` |

## 3. Area 18

| PC | Name | Size | Root | PSX twin | What |
|---|---|--:|---|---|---|
| `0x402CA0` | `Area18_SetRecordByte` | `0x32` | init | `0x801F2C04` | the byte `+4` of the record `Area_StateRecords[Game_AreaNumber]` (`0x668D80`, a pointer per area) points at: 0 when `Cond_ByteFA` is 8, else 2 |
| `0x402CE0` | `Area18_SkipScript` | `0x19` | handler 0 | `0x801F2C68` | unless `Field_State`'s byte `+0x89` is 2, `MoveScript_Object`'s script position `+0xA` + 9 (the script skips nine bytes) |
| `0x402D00` | `Area18_ClearCells` | `0x5F` | handler 1 (and area 19's 6) | `0x801F2CA8` | `AreaMap_SetByte(x, z, 0)` for x `0x1F`, `0x20` of rows `0x4C`, `0x4D`, 3, 2 |

Tables: `Area18_Handlers` `0x5E6680` (2). PSX twins by
`names/area_records.toml`'s descriptor slots (hypotheses, not
disassembled).

## 4. Area 19

| PC | Name | Size | Root | PSX twin | What |
|---|---|--:|---|---|---|
| `0x402D60` | `Area19_CameraOut` | `0x11` | handler 0 | `0x801F2C04` | `Camera_Distance + 0xA00` (a word, no bound), `MapView_Redraw = 2` |
| `0x402D80` | `Area19_CameraIn` | `0x11` | handler 1 | `0x801F2C2C` | `Camera_Distance - 0xA00`, `MapView_Redraw = 2` |
| `0x402DA0` | `Area19_PlaceKind2` | `0x9` | handler 5 | `0x801F2CB4` | `Kind2_Place(4)` |

Tables: `Area19_Handlers` `0x5E6B60` (8: handlers 2..4 are area 3's
`0x4012D0..0x4012F0`, 6 is `Area18_ClearCells`, 7 is `0x40CE10`).

## 5. Area 20

`Area20_PickFieldObject` `0x402DB0` (`0xCB`, init, PSX `0x801F2D2C`):
`Area29_PickFieldObject`'s body ([`worldmap_area.md`](worldmap_area.md)
section 1) over `Area20_Weights` `0x5E6C4C` and `Area20_Cells` `0x5E6C3C`:
one of the first eight field objects kept by a weighted roll of `Rand() &
0x3F`, the other seven cleared, the kept one placed at one of eight cells
with its elevation; `Field_EdgeBits` = the leader's `+0x134` - 5. Tables:
`Area20_Cells` (16 bytes), `Area20_Weights` (8).

## 6. Area 21 (choice handlers)

| PC | Name | Size | Root | What |
|---|---|--:|---|---|
| `0x402E80` | `Area21_ChoicePay` | `0x3C` | choice 1 | choice not 0: message `0x18`; the money `0x904058` below 20: message `0x17`; else `0x591BC0(20)` (the money take) and message `0x11` |
| `0x402EC0` | `Area21_ChoiceMessage` | `0x17` | choice 2 | message `Area21_Messages[(s8) choice]` |
| `0x402EE0` | `Area21_ChoiceTrade` | `0xAF` | choices 3, 4 | choice not 0: the counter `0x90384B` + 1, wrapping at 8 (message `0x34`, else `0x31`); choice 0: without item 3 of category 0 (`Inventory_Count`) message `0x32`; else n = (counter + 1) * 10, `Crt_sprintf(Text_Records row 1, "%d", n)`, message `0x30`, the money give `0x591BE0(n, 0)`, the inventory take `0x591B60(0, 3, 1)`, `Flags_Set(*0x929ED0, 0x12)` - an item traded for a rising sum |

Tables: `Area21_Choices` `0x5E7268` (5: entry 0 is the bare `ret`
`0x437CC0`, 3 and 4 the same function), `Area21_Messages` `0x5E72C4`.

## 7. Area 22 and the nine areas' shared choice

| PC | Name | Size | Root | PSX twin | What |
|---|---|--:|---|---|---|
| `0x402F90` | `Area22_ChoiceA` | `0x28` | choice 0 | | no message; `0x903848` = `0x1E` (choice 0) or `0x32` (1) |
| `0x402FC0` | `Area22_ChoiceB` | `0x28` | choice 1 | | `0x3C` or `0x46` |
| `0x402FF0` | `Area22_ChoiceC` | `0x28` | choice 2 | | `0xB` or `0x14` |
| `0x403020` | `Area22_ClearCells` | `0x30` | handler 0 | `0x801F2CC4` | cells (3, 7), (3, 8), (4, 7), (4, 8) to 0 |
| `0x403050` | `Area22_ArmTailOnYes` | `0x28` | nine areas (section 1) | | no message; on choice 0 the mode tail kind `0xA` armed (`0x9039F3 = 0xA`, `0x9039F4 = 5`, `0x9039F5 = 0xFF`: `Field_ModeTailKinds` slot 10) |

`0x903848` is a movement-script variable the choices leave for the script.
Tables: `Area22_Choices` `0x5E7DB8` (3), `Area22_Handlers` `0x5E7DE4` (1).

## 8. Area 23

| PC | Name | Size | Root | PSX twin | What |
|---|---|--:|---|---|---|
| `0x403080` | `Area23_ClearCells` | `0x30` | choice 2, handler 0 | `0x801F2C88` | cells (0x48, 0xB), (0x49, 0xB), (0x48, 0xC), (0x49, 0xC) to 0 |
| `0x4030B0` | `Area23_SetCells` | `0x3C` | choice 3, handler 1 | `0x801F2CE0` | the same four back: row `0xB` `0xC0`, row `0xC` `0xA1` |
| `0x4030F0` | `Area23_CameraUp` | `0x10` | choice 4, handler 2 | `0x801F2D38` | `Camera_ShiftY - 0x12`, `MapView_Redraw = 2` |
| `0x403100` | `Area23_CameraDown` | `0x10` | choice 5, handler 3 | `0x801F2D60` | `Camera_ShiftY + 0x12`, `MapView_Redraw = 2` |

Tables: `Area23_Choices` `0x5E8730` (7: `Area22_ArmTailOnYes`, `0x425C30`,
then the handler array), `Area23_Handlers` `0x5E8738` (5; the fifth
`0x40F530`).

## 9. Areas 24 and 25

- `Area24_NoMessage` `0x403110` (`0xA`, choices 0 and 1 of
  `Area24_Choices` `0x5EA040`): message `0xFFFF`.
- `Area25_SpawnEffect` `0x403120` (`0x57`, handler 0 of `Area25_Handlers`
  `0x5EACD4`, PSX `0x801F2C04`): every fourth frame, `Effect_FindFree` to
  `0x903850`; a slot: its `Effect_Objects` record `+0 = 1`, kind `+5 = 0xB`,
  dword `+0x1C = 6`, variant `+7` 1 while the count byte `0x90384A` is below
  `0x14`, else 3.

## 10. Area 26

| PC | Name | Size | Root | PSX twin | What |
|---|---|--:|---|---|---|
| `0x403180` | `Area26_ChoiceMessage` | `0x2E` | choice 0 | | message `Area26_Messages[(s8) choice]`; `0x903848` = `0xBE` / `0xBF` |
| `0x4031B0` | `Area26_ChoiceSetVar` | `0x28` | choice 1 | | no message; `0x903848` = 8 / `0xF` |
| `0x4031E0` | `Area26_Flag29Set28Clear` | `0x1F` | choice 2, handler 0 | `0x801F309C` | `Flags_Set(row, 0x29)`, `Flags_Clear(row, 0x28)`, the row `*0x929ED0` read for each |
| `0x403200` | `Area26_Flag29Clear` | `0x11` | choice 3, handler 1 | `0x801F30D4` | `Flags_Clear 0x29` |
| `0x403220` | `Area26_Flag28Set` | `0x11` | 4 / 2 | `0x801F30FC` | `Flags_Set 0x28` |
| `0x403240` | `Area26_Flag28Clear` | `0x11` | 5 / 3 | `0x801F3124` | `Flags_Clear 0x28` |
| `0x403260` | `Area26_Flag2BSet` | `0x11` | 6 / 4 | `0x801F314C` | `Flags_Set 0x2B` |
| `0x403280` | `Area26_Flag2BClear` | `0x11` | 7 / 5 | `0x801F3184` | `Flags_Clear 0x2B` |
| `0x4032A0` | `Area26_Flag2CSet` | `0x11` | 8 / 6 | `0x801F31AC` | `Flags_Set 0x2C` |
| `0x4032C0` | `Area26_Flag2CClear` | `0x11` | 9 / 7 | `0x801F31D4` | `Flags_Clear 0x2C` |
| `0x4032E0` | `Area26_GiveItem` | `0x28` | 10 / 8 | `0x801F31FC` | `Inventory_Add(4, 3, 1)` (a fourth word 0 pushed, not read), `Sound_PlayEffect(0x106)`, `Flags_Set 0x33` |
| `0x403310` | `Area26_ResetCamera` | `0x19` | 13 / 11 | `0x801F3290` | `Camera_ShiftY = 0`, `MapView_Redraw = 2`, `Music_FadeOutStop(0xA)` |
| `0x403330` | `Area26_SpawnEffect` | `0x42` | 14 / 12 | `0x801F32C8` | as `Area11_SpawnEffect`: `Effect_Spawn(1, 0, Area26_EffectKinds[first member], the leader's x, z)` from the leader, the slot to `Sprite_Current[0xB]` (read again) |
| `0x403380` | `Area26_Flag2ESet` | `0x11` | 15 / 13 | `0x801F3348` | `Flags_Set 0x2E` |
| `0x4033A0` | `Area26_PlaceEffect` | `0x53` | 16 / 14 | `0x801F3370` | `Effect_FindFree`; a slot: its record at (7, 6.5) cells (`+0x34 = 0x70000`, `+0x38 = 0x68000`), `+0 = 1`, kind `+5 = 0x33`, `+0x3C` = the elevation there `<< 16` |

Tables: `Area26_Choices` `0x5EC558` (17: the first two, then the handler
array), `Area26_Handlers` `0x5EC560` (15; 9 and 10 are `0x409970` and
`0x409960`, outside the band), `Area26_Messages` `0x5EC5E4`,
`Area26_EffectKinds` `0x5EC5E8`.

## 11. The fuzz

`BOF3X_SHADOW=area_w0b` (`src/game/area_w0b_fuzz.cpp`): ten `Run` calls under
the one shadow name, `Group::area` each area's number, the real descriptors
and tables in place. Shapes as the roots say: `kState` for area 16's
record, hook and state functions, `kCallee` for its two step functions and
three draws (arguments from the group's `args`), `kInit`, `kHandler`,
`kChoice` for the rest (a function that is both a choice and a handler is
fuzzed as a choice: the message word is logged after). 4,000 rounds per
function (area 20's init 8,000).

**Area 16's group** swaps its six state tables (`DataTable`, 20 entries) and
adds 15 regions: the map's mode byte, `Draw_PassFlags`, `Gfx_PacketNext`
and a packet buffer of the fuzz's, `Prim_VertexScratch`, `Game_Mode`, the
button map, four map items and four names of the fuzz's, `Text_Records`'
first four rows, the area-text offset, `0x903A79`, and area 16's data tables
(`0x5E5BE0..0x5E5F5B`, the directions and animations, the drift UV - put
back to the exe's bytes two rounds in three). Listed beyond the standard set:
area 16's own called functions (the two step functions `kPhase`; the draws
with their pushed words masked where the original pushes a stale high half),
`ScriptFlags_Set40` / `_Clear40`, `Item_NamePtr` (answering one of the four
names), `WorldMap_PinSprite`, `Effect_Release`, `Gfx_CommitPrim` (moving the
packet cursor), `WorldMap_DrawNeedle`, `WorldMap_RecordIndex`,
`Prim_SetTexture`, and the standard ones that write what the caller reads
after: `AreaMap_ByteAt` (answering the cell kinds `0xA1` / `0xA0` / `0xAE`
mostly), `Field_CellHasEvent` (words masked), `MapView_ItemHalfAt` (none a
third of the time, else one of the items), `MapView_LinkPrimAt`, the GPU
setters (writing the primitive's bytes), `Gte_PrimDepths4_10`, and
`Gte_RotTransPers4` with its ten pushed words (the four vectors dereferenced,
the two stack locals not logged). Its `disturb` moves the mode byte, the pass
flags, both flag words, the hook's state byte, `Game_Mode`, `Cond_ByteFA`,
the place word, the leader's cell words and the party set; its `settle`
keeps `+1` inside the plate table (read after the plate run's calls) and
re-plants the leader's cell record (the hook's search has no bound);
`phase_span` 5. Seeds: every boundary of section 2 as
[`worldmap_area.md`](worldmap_area.md) section 7 seeded area 33's, plus the
hook's state byte (0, 1, 2, `0xFF`, `0x80`), the place planted in a message
row, the found cell's id one of the three sets', the items' held bytes, a
list ended early (`0xFF`, item `0x16`), the map width below `0x18` for the
cell mark (so any record lands in the 8 KiB block), `0x903A79` 9 / 8.

**Areas 18..26:** area 18 adds the area record's byte and
`MoveScript_Object` (seeded to a field object or party record); area 19
lists `Kind2_Place`; area 20 its two tables (the first roll at each running
sum and one below, a zero or small weight table for "none kept"); area 21
the three unowned callees, `Crt_sprintf` and `Inventory_Count` (its word 0 a
quarter of the time), the money at `0x13` / `0x14` / `0x15`, the counter at
7 / 8; areas 21 and 26 a `disturb` that moves the flag row pointer
`0x929ED0` (never followed; the recorders log it) so a row not read again
shows; areas 25 and 26 `Effect_Objects`' first eight records with
`Effect_FindFree` answering 0..7 or `0xFF`; area 26 `Effect_Spawn` `kByte
0xFE..0x02` as area 11. The choice byte `0x7DEE67` 0, 1, 2, `0xFF`, `0x80`,
`0x7F`, `0x81`.

**Result (in this worktree):** 0 mismatches in every run; area 16 100,000
rounds, 520,305 calls; 18 12,000 / 32,000; 19 12,000 / 4,000; 20 8,000 /
19,370; 21 12,000 / 4,285; 22 16,000 / 16,000; 23 20,000 / 32,000; 24 4,000
/ 0; 25 4,000 / 3,044; 26 60,000 / 67,530. Every state table entry reached
(the plate's five 758..851 times, the frame's, box's and records' 971..5,021).
`BOF3X_SHADOW='*'`: section 13.

## 12. What reaches it, defects, calls across groups

- **Reach:** no function of `0x401B80..0x403400` is in any call trace or
  hidden-reach list (`analysis/area_funcs.tsv`'s live column is empty for the
  group). Area 16 runs while its world map is the current area (record 0);
  the rest as their areas' scripts call them. Fuzz only.
- **Latent defects (described, not fixed; the fuzz keeps inside them):**
  1. `Area16_PlaceMessage`'s cell search has no bound: a leader cell with no
     record reads on through `.data` (the plate searches of both maps have
     the same shape, D74).
  2. `Area16_PlaceMessage`'s name set 3 (an id in no set) reads the plate
     state table's code-pointer bytes as item ids.
  3. `Area16_Record4MarkCell` indexes `Area16_Cells` by `+0xB` unchecked and
     writes `AreaMap_Bytes` at `width * z + x` unchecked.
  4. `Area16_Record8Place` indexes its direction and animation tables by
     `+8` unchecked.
  5. The unchecked `.data` dispatches (`+1`, `+2`, `+3`) of every state
     machine, as area 33's.
  6. `Area21_ChoiceMessage` and `Area26_ChoiceMessage` index their message
     tables by the signed choice byte; `Area26_SpawnEffect` its kinds by a
     member id.
  7. Area 20's init has area 29's "none kept" exit, reachable only with a
     weight table summing below 64 (the shipped one: not read here).
- **Cross-group raw-address calls:** `0x591BC0`, `0x591BE0`, `0x591B60`
  (engine, nobody's this wave). Area 16's tables name `0x4253C0` (AR4A),
  `0x40C490` (AR1E), `0x408990` (AR1B) - reached through the tables, not
  called. Named and ours: `WorldMap_DrawNeedle`, `WorldMap_RecordIndex`,
  `Field_CellHasEvent`, `Item_NamePtr`, `Kind2_Place`, `Effect_FindFree` and
  the standard set. Capcom's by name: `Rand`, `Crt_sprintf`, `Effect_Spawn`.
- `analysis/calltrace/entries_logic.txt`: 59 lines appended (the other two,
  `004022E0 1C5` and `004024B0 BC`, were there); `004020B0 12` and `00402570
  58` are smaller than the host lines `004020B0 227` and `00402570 1240`.

## 13. Controls

(Filled in by the controls run.)
