# World 1, areas 65 and 67: the world map's fourth copy, and area 67's handlers

**Status:** IN PROGRESS (2026-09-28) - 49 functions ours
(`src/game/area_w1e.cpp`, shadow name `area_w1e`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)), one `Run` per area:
0 mismatches in 196,000 rounds (in this worktree); 95 controls planted, 95 refused (93 by a count, 2 by a fault, the fault's variant by a count) (section 7). Fuzz only: no
recorded route reaches the band (section 6). No divergence.

Group AR1E of round ten's fourth wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) section 12): the
band `0x40B8C0..0x40CEF0`, whole areas as `tools/area_rows.py --groups` cuts
them (at the staging tip the tool prints this group as its `AR1B` row: its
world-1 letters shift as areas become ours; the merged `area_w1b` is areas
42..47, a different group). What each area *is* in the story is not read
here: the names come from what the code does.

## 1. The band, the areas, the function count

**49 starts, none ours before, 49 taken; none dropped, none added.**
`analysis/area_funcs.tsv` lists 43 of them; the tool's walk (`--groups`,
`--unit AREA065 / AREA067 --clones`) lists all 49. The six the TSV lacks are
real functions, each read whole:

- `0x40C490` - the record `+8` effect's state 2 of **ten world maps** (the
  Record8States tables of areas 16, 33, 45, 65, 87, 88, 115, 121, 151, 152
  name it: `xref` of the image finds exactly those ten dwords). It lies in
  area 65's block between `Area65_Record8Place` and `Area65_Record4Run`;
  round eight's docs and AR0B / AR0C / AR1B already called it "AR1E's".
  Named `Area65_Record8Move` (the copies are named per area, and the body is
  area 65's by address, as `Area45_Record4Tick` is area 45's).
- `0x40CAB0`, `0x40CAC0`, `0x40CDE0`, `0x40CE10` - area 67's handlers 0, 1,
  12 and 15 (each also in another area's array, section 3).
- `0x40CED0` - `Field_ObjectTriggers` id 31 (the dword `0x662E98` =
  `0x662E1C + 31 * 4`, the only reference in the image), placed in area 67's
  block by address.

Every start follows `nop` padding or a `ret`; every gap between them is
padding. Area 66 has no code of its own (`--unit AREA066`: no exclusive
function). Each extent is the tool's and agrees with the reading; the clone
rows are the tool's, each read against the disassembly (they match call site
for call site).

| Area | Descriptor | Roots | Block | Fns |
|--:|---|---|---|--:|
| 65 | `0x6046D0` (a choice table `0x6046CC` -> `0x41D270` and an init `0x40FC40`, both outside the band) | `WorldMap_Records` record 3 (`+0`, `+4`, `+8`, `+0xC`, `+0x10`: `0x653964..0x653974`), `WorldMap_FieldHooks` entry 3 (`0x662DFC`), six state tables in its data block | `0x40B8C0..0x40CA22` | 26 |
| 67 | `0x607000` (PSX `0x801F5340`) | `Area67_Choices` (3), `Area67_Handlers` (19), object trigger 31 | `0x40CA30..0x40CEEC` | 23 |

**The tool's two known gaps** (round10 doc section 7), looked for: no start
without padding or a `ret` before it; no tail kind armed through a register
(the one tail this band arms, `Area67_Trigger31`'s kind `0x2C`, is an
immediate, and its slot is engine code `0x56DE50`, not the band's).

## 2. Area 65: the world map's fourth copy

Area 65 is `WorldMap_Records` record 3 ([`worldmap_area.md`](worldmap_area.md)
section 5). **Its code is area 45's** ([`area_w1b.md`](area_w1b.md) section 5,
itself area 16's, [`area_w0b.md`](area_w0b.md) section 2), read whole and
compared pair by pair with capstone (the scratch `cmp.py` / `cmp2.py`): 24 of
the 25 pairs have the same instruction count and mnemonics, every jump and
call target maps to the same offset in the corresponding copy (0 that do
not), and the other operands differing are the table addresses - **but for
these**:

| What | Area 65 | Area 45 | Area 16 |
|---|---|---|---|
| `PlateStart`'s animation bank | `0x53` (`0x40BB25`) | `0x3B` | `0xF` |
| `PlaceMessage`'s name sets | **two** records of **6** bytes (id, five items) at `0x604874`, "none" = set 2 | three of 5 bytes (id, four items), none = 3 | as 45 |
| `PlaceMessage`'s text rows | **five** `Text_Records` rows (`0x904CE0..0x904D80`) | four (`..0x904D60`) | four |
| `DrawHud`'s region label cell | `0x803584` | `0x803584` | `0x803588` |

The name-set change is a different loop (`add eax, 6; cmp eax, 0x604880`,
the items at `[set * 6 + 0x604875]`, the row loop to `0x904D80`): one more
instruction (`lea ecx, [ebp + ebp*2]` before the `* 2`), four more bytes
(`0x178` against `0x174`), and the last two call sites four bytes later
(`+0x117`, `+0x158`). The plate bank is why area 65 has a plate start of its
own, as area 45 does. Every other function is area 45's over area 65's
tables, and ours is area 45's code over them, with area 45's one policy
difference: the six state dispatchers abort past their tables.

| Area 65 | Area 45's | Size | Root |
|---|---|--:|---|
| `Area65_PlaceMessage` `0x40B8C0` | `0x407B40` | `0x178` | `WorldMap_FieldHooks` entry 3 |
| `Area65_PlateRun` `0x40BA40` | `0x407CC0` | `0xD6` | record 3 `+0` |
| `Area65_PlateStart` / `Show` / `Grow` / `Hold` / `Shrink` `0x40BB20..0x40BD70` | `0x407DA0..0x407FF0` | `0x4E`, `0x142`, `0x41`, `0x58`, `0x50` | `Area65_PlateStates` 0..4 |
| `Area65_HudRun` / `HudFrame` `0x40BDC0` / `0x40BDE0` | `0x408040` / `0x408060` | `0x12`, `0xA` | record 3 `+0xC`; `Area65_HudStates` 1 |
| `Area65_FrameStep` / `SlideIn` / `Hold` / `SlideOut` `0x40BDF0..0x40BE70` | `0x408070..0x4080F0` | `0x12`, `0x21`, `0x28`, `0x41` | called; `Area65_FrameStates` 1..3 |
| `Area65_BoxStep` / `SlideIn` / `Hold` / `SlideOut` `0x40BEC0..0x40BFC0` | `0x408140..0x408240` | `0x12`, `0x62`, `0x6E`, `0x57` | called; `Area65_BoxStates` 1..3 |
| `Area65_DrawFrame` / `DrawSprite` `0x40C020` / `0x40C1F0` | `0x4082A0` / `0x408470` | `0x1C5`, `0xBC` | called |
| `Area65_DrawHud` `0x40C2B0` | `0x4086D0` | `0x58` | called |
| `Area65_Record8Run` / `Record8Place` `0x40C310` / `0x40C330` | `0x408730` / `0x408750` | `0x12`, `0x154` | record 3 `+8`; `Area65_Record8States` 1 |
| `Area65_Record8Move` `0x40C490` (shared by ten maps) | - | `0x4B` | the ten maps' Record8States 2 |
| `Area65_Record4Run` / `Record4MarkCell` `0x40C4E0` / `0x40C500` | `0x4088B0` / `0x4088D0` | `0x12`, `0xB5` | record 3 `+4`; `Area65_Record4States` 0 |
| `Area65_DrawDrift` `0x40C5C0` | `0x4089A0` | `0x462` | record 3 `+0x10` |

**`Area65_Record8Move` `0x40C490`** (the record `+8` effect, kind `0x16`,
after `Record8Place` has aimed it): `+0` and `+0xB` read once. `+0xB` 0: `+0`
bit 7 clear sets `+0xB = 1`; either way the effect moves on. `+0xB` set: `+0`
bit 7 set releases it (a tail jump to `Effect_Release`), clear moves it on.
Moving on: `+0x34 += +0xC`, `+0x38 += +0x10` (the direction words
`Record8Place` stored), `Sprite_ScriptTick` (its answer not read), then a
tail jump to `Sprite_UpdateScreen`.

**Area 65's tables** (`[[data]]`; in its own data block `0x6043C8..0x604963`,
around its choice table and descriptor `0x6046CC..0x604713`; `xref` of each
address: read only by area 65's code): `Area65_PlateAnims` `0x6043C8` (11),
`Area65_Cells` `0x6043F4` (**182** x 4 - then area 65's choice table
`0x6046CC` and descriptor `0x6046D0..0x604713`; area 45's doc counted its
"265" over its descriptor the same way), `Area65_PlaceMessages` `0x604714`
(11 rows), `Area65_NameSets` `0x604874` (2 x 6), `Area65_PlateStates`
`0x604880` (5), `Area65_HudStates` `0x604894` (2), `Area65_FrameStates`
`0x60489C` (4), `Area65_BoxStates` `0x6048AC` (4), `Area65_Sprites`
`0x6048BC` (22 x 4), `Area65_Buttons` `0x604914` (6 x 4; the second legend
reads eight), `Area65_Record8States` `0x60492C` (3: `0x4253C0`,
`Area65_Record8Place`, `Area65_Record8Move`), `Area65_Directions` `0x604938`
(4 x 2 words), `Area65_Record8Anims` `0x604948` (4 x 2), `Area65_Record4States`
`0x604950` (2: `Area65_Record4MarkCell`, `Area45_Record4Tick`),
`Area65_DriftUV` `0x604958` (12). The HUD, frame and box tables' state 0
entries are round eight's shared `WorldMapHud_Start`, `WorldMap_FrameWait`,
`WorldMapHud_BoxWait`.

## 3. Area 67: three choices, nineteen handlers, a trigger

| PC | Name | Size | Root | PSX twin | What |
|---|---|--:|---|---|---|
| `0x40CA30` | `Area67_ChoiceMark6` | `0x23` | choice 0 | | message word `0x7DEE48` = `Area67_MessagesA` `0x607044` by the s8 choice (unchecked); choice 5 also sets the byte `0x903848` to 6 |
| `0x40CA60` | `Area67_ChoiceMessage` | `0x17` | choice 1 | | message = `Area67_MessagesB` `0x607050` by the s8 choice |
| `0x40CA80` | `Area67_ChoiceMark5` | `0x27` | choice 2 | | message = `Area67_MessagesC` `0x60705C` by the s8 choice; choices 0 and 1 (signed tests) set `0x903848` to 5 |
| `0x40CAB0` | `Area67_SetBit24` | `0xA` | handler 0 (and area 108's handler 12 / choice 15) | `0x801F3EC4` | `Sprite_Current +0x24 |= 0x10` |
| `0x40CAC0` | `Area67_Object0XUp` | `0xB` | handler 1 (and area 131's) | `0x801F3EE4` | field object 0's `+0x34` (`0x7DEEB4`, 16.16) `+= 0x800` |
| `0x40CAD0` | `Area67_SpawnEffect2D` | `0x40` | handler 2 | `0x801F3F00` | `Effect_FindFree`; a slot: `+0 = 1`, kind `+5 = 0x2D`, `+0x34` / `+0x38` the running object's |
| `0x40CB10` / `0x40CB60` / `0x40CBB0` | `Area67_Member0SpawnAt` / `1` / `2` | `0x46`, `0x46`, `0x49` | handlers 3..5 | `0x801F3F80`, `F400C`, `F4098` | member k's record (`ObjTrio + k * 0x14C`) becomes `Sprite_Current`; `Effect_SpawnAt(3, 0, Area67_SpawnAtArg[the member's character id from the party list 0x904062 + k], +0x34, +0x38, +0x3C)`; an answer other than `0xFF` to `Sprite_Current +0xB` (read again) |
| `0x40CC00` / `0x40CC50` / `0x40CCA0` | `Area67_Member0Spawn3` / `1` / `2` | `0x42`, `0x42`, `0x46` | handlers 6..8 | `0x801F4124`, `F41A4`, `F4224` | the same shape with `Effect_Spawn(3, 0, Area67_SpawnAtArg[id], the words +0x2E, +0x30)` |
| `0x40CCF0` / `0x40CD40` / `0x40CD90` | `Area67_Member0Spawn1` / `1` / `2` | `0x42`, `0x42`, `0x46` | handlers 9..11 | `0x801F42A4`, `F4324`, `F43A4` | `Effect_Spawn` kind 1 with `Area67_SpawnArgB` `0x604BC0` |
| `0x40CDE0` | `Area67_Object0XDown` | `0xB` | handler 12 (and area 131's) | `0x801F4424` | object 0's `+0x34 -= 0x800` |
| `0x40CDF0` / `0x40CE00` | `Area67_Object1XUp` / `Down` | `0xB` | handlers 13, 14 | `0x801F4440`, `F445C` | object 1's `+0x34` (`0x7DEF58`) `+=` / `-= 0x800` |
| `0x40CE10` | `Area67_MusicFade10` | `0x9` | handler 15 of areas 67, 7, 19, 130 | `0x801F4478` | `Music_FadeOutStop(10)` |
| `0x40CE20` | `Area67_LeaderSpawn4` | `0x42` | handler 16 | `0x801F449C` | `Effect_Spawn` kind 4 at the leader with `Area67_SpawnArgC` `0x604BC8` |
| `0x40CE70` | `Area67_ResetOn6` | `0x4F` | handler 17 | `0x801F451C` | `Field_ActiveMember +0x80` bit 0 cleared (the byte read before the test); the leader's `+0x89` 6: `ScriptFlags_Set40`, `MoveScript_Var7 = 4`, the bytes `0x903848..0x90384B` and `0x8034E5` zeroed |
| `0x40CEC0` | `Area67_ClearCell` | `0xF` | handler 18 | `0x801F45BC` | `AreaMap_SetByte(0x25, 0xB, 0)` |
| `0x40CED0` | `Area67_Trigger31` | `0x1D` | object trigger 31 | | `ScriptFlags_Set40`; the mode tail armed: kind `0x2C` (the engine's `0x56DE50`), state 0, `0x9039F5 = 1`; al 0 |

`Area67_Member2SpawnAt` and `_Member2Spawn*` read the third id as a dword
`& 0xFF` (the same byte). The positions are read before `Sprite_Current` is
set; the argument byte is pushed with a stale high half (the harness masks
it to 8 bits). PSX twins are `names/area_records.toml`'s descriptor slots
(handlers 0..18 in order); the choices and the trigger have no pairing there.

Tables: `Area67_Handlers` `0x606F20` (19; its movement scripts follow),
`Area67_Choices` `0x606FF4` (3), `Area67_MessagesA` / `B` / `C` `0x607044` /
`0x607050` / `0x60705C` (u16 by the s8 choice), and three byte tables by a
character id in **area 66's** data block after its descriptor `0x604B70` -
`Area67_SpawnAtArg` `0x604BB8`, `Area67_SpawnArgB` `0x604BC0`,
`Area67_SpawnArgC` `0x604BC8` - each read only by area 67's handlers (`xref`:
the nine and one code references are all in `0x40CB10..0x40CE70`).

## 4. The fuzz

`BOF3X_SHADOW=area_w1e` (`src/game/area_w1e_fuzz.cpp`): two `Run` calls
under the one shadow name, `Group::area` 65 and 67, the real descriptors and
tables in place. `BOF3X_AR1E_AREA=n` runs one area's group alone (the
controls script's shortcut).

- **Area 65** (4,000 rounds a function): area 45's group
  ([`area_w1b.md`](area_w1b.md) section 8) with area 65's tables - the six
  state tables as `DataTable`s; the packet buffer, items and names the
  fuzz's own; area 65's tables as regions, put back to the exe's bytes two
  rounds in three - two regions, `0x6043C8..0x6046CB` and
  `0x604714..0x60487F`, so area 65's own choice table and descriptor stay the
  image's; `settle` keeping `+1` inside the plate table and re-planting the
  leader's cell record; the name-set seeds reshaped to two sets of five
  items (any of the five ended early), the place rows 11, the plate places
  11, the cell records 182. `Area65_Record8Move` seeded with `+0xB` 0 or
  set and `+0` bit 7 set or clear (the four ways).
- **Area 67** (4,000): `Effect_Spawn` and `Effect_SpawnAt` listed by their
  addresses (Capcom's) as `kByte 0xFE..0x02`, so the handlers' `0xFF` test
  sees "none" a fifth of the time; `Effect_FindFree`, `ScriptFlags_Set40`;
  `Field_ActiveMember` and the first eight effect records as regions.
  Seeds: `Field_ActiveMember` a party record or a field object; the party
  list's ids mostly 0..7; the choice byte 0..6, `0xFF`, `0x80`, `0x7F`; the
  leader's `+0x89` 6 half the time and its neighbours. The trigger's
  arguments a field object and the bank.

**Result (in this worktree):** 0 mismatches; area 65 104,000 rounds, 543,171
calls to the stand-ins, every state-table entry reached (the plate's five
760..860 times each, the HUD's, frame's, box's, record 8's three - the
shared `0x4253C0` and `Area65_Record8Move` included - and record 4's two -
`Area45_Record4Tick` included); area 67 92,000 rounds, 57,165 calls.
`BOF3X_SHADOW='*'`: exit 0, 406 self-test lines, no mismatch, `inject: 4603 ours` (all 49 of this group's injected); this group's counts in that run 104,000 / 527,596 and 92,000 / 57,149 (the runs share the harness's random stream).

## 5. Latent defects (described, not fixed)

1. Area 65 has area 45's: the unbounded cell and plate searches (D74's
   shape: a cell or place in no record reads on through the image until a
   match or a fault), the name set "none" (2) reading the plate state
   table's first bytes as items, the unchecked record indexes
   (`Record8Place`'s `+8`, `Record4MarkCell`'s `+0xB` - past 182 it reads
   the choice table and the descriptor as cells), the unchecked `.data`
   dispatches (ours aborts past each of the six tables, as AR1B's does).
2. Area 67's choice handlers index their message tables by the signed choice
   byte, unchecked; the member handlers index their argument tables by a
   character id, unchecked (a byte, so at most 255 past: image data, no
   fault; ours reads the same).

**Numbered 2026-09-28** ([`round-10-cleanup.md`](round-10-cleanup.md) item 2):
the defects above are D133 (the dispatchers), D134 (tables back to back), D136
(reads and writes by an unchecked byte or count), D143 (the world-map copies'
searches) in [`known-defects.md`](known-defects.md).

## 6. What reaches it, calls across groups

- **Reach:** no recorded route reaches the band - `hidden_reached_combat`,
  `_shop`, `_worldmap.json` hold no entry in `0x40B8C0..0x40CEF0`, and the
  world-map route's call counts and hidden trace name none of the 49 (the
  route plays area 33's map). Fuzz only.
- **Cross-group raw-address calls: none.** Every callee is named: ours
  (`AreaMap_ByteAt`, `AreaMap_SetByte`, `Msg_OpenScript`, `Item_NamePtr`,
  `ScriptFlags_Set40` / `_Clear40`, `Sprite_*`, `Gpu_*`, `Gfx_CommitPrim`,
  `Gte_*`, `MapView_*`, `Prim_SetTexture`, `Field_CellHasEvent`,
  `WorldMap_PinSprite`, `WorldMap_DrawNeedle`, `WorldMap_RecordIndex`,
  `Text_DrawAt`, `Effect_FindFree`, `Effect_Release`, `Music_FadeOutStop`)
  or Capcom's with a signature (`Effect_Spawn`, `Effect_SpawnAt`). Tables
  name `0x4253C0` (AR4A), `Area45_Record4Tick` (AR1B) and round eight's
  shared HUD states - reached through the tables, not called. None of SX2's
  twelve.
- **Inbound (for the rebinding pass):** the Record8States tables of areas
  16 (`0x5E6010`), 33 (`0x5EF690`), 45 (`0x5F7708`), 87, 88, 115, 121, 151,
  152 name `Area65_Record8Move` `0x40C490` raw; area 108's arrays
  (`0x61E588`) name `Area67_SetBit24` `0x40CAB0`; area 131's (`0x62990C`,
  `0x629908`) name `Area67_Object0XUp` and `Area67_Object0XDown`; areas 7
  (`0x5E0364`), 19 (`0x5E6B7C`) and 130 (`0x628C40`) name
  `Area67_MusicFade10` `0x40CE10`. No code outside the band calls into it.
- **Harness:** no standard-set column moved; no harness edit.
- `analysis/calltrace/entries_logic.txt` (main checkout): 47 lines appended
  (`0040C020 1C5` and `0040C1F0 BC` were there); `0040BDF0 12` and
  `0040C2B0 58` are smaller than host lines already there (`0040BDF0 227`,
  `0040C2B0 1ECD`).

## 7. Controls

Planted one at a time in `area_w1e.cpp` by a script (the scratch
`controls.py`, not committed): each anchored on a string the file holds once;
plant, rebuild (checking `area_w1e.cpp` recompiled), run the area's group
alone (`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=area_w1e BOF3X_AR1E_AREA=n`),
restore; after the last, a rebuild and a clean full run (exit 0, 0
mismatches in both runs). **95 planted, 95 refused**: 93 by a count (exit
3), 2 by a fault - D5 and D5b, the place hook's cell words not read again
after `AreaMap_ByteAt`: with a stale word the unbounded search finds no
record and runs off the image, as the original would with those words -
and the fault's near variant D5d (the same stale read with the search
stopped at the table's end) refused by a count. At least one per function;
the shared helpers (`MemberSpawnAt`, `MemberSpawn`, `ChoiceMessage`) are
refused in every handler that uses them. None stood on the first run.

| # | Area | planted | refused in rounds (of 4,000) |
|---|--:|---|---|
| D1 | 65 | PlaceMessage: state 1 handled as 2 | Area65_PlaceMessage 723 |
| D2 | 65 | PlaceMessage: set + 0x17 | Area65_PlaceMessage 822 |
| D3 | 65 | PlaceMessage: sets searched by 5 (area 45's) | Area65_PlaceMessage 541 |
| D4 | 65 | PlaceMessage: four text rows (area 45's) | Area65_PlaceMessage 772 |
| D5 | 65 | PlaceMessage: the cell words not read again | a fault (exit 0xC0000005: the unbounded cell search runs off the image) |
| D5b | 65 | PlaceMessage: both cell words not read again | a fault (exit 0xC0000005: the unbounded cell search runs off the image) |
| D5c | 65 | PlaceMessage: the id from the record's byte 2 | Area65_PlaceMessage 546 |
| D5d | 65 | PlaceMessage: x not read again, the search stopped at the table's end | Area65_PlaceMessage 1 |
| D6 | 65 | PlaceMessage: items by set * 5 | Area65_PlaceMessage 534 |
| D7 | 65 | PlaceMessage: an unseen name 0x3E | Area65_PlaceMessage 727 |
| D8 | 65 | PlaceMessage: rows to 10 | Area65_PlaceMessage 68 |
| D9 | 65 | PlateRun: kind 1 on 0xA2 | Area65_PlateRun 1237 |
| D10 | 65 | PlateRun: the next state | Area65_PlateRun 4000 |
| D11 | 65 | PlateStart: area 45's bank 0x3B | Area65_PlateStart 4000 |
| D12 | 65 | PlateShow: kind 2 animation 2 | Area65_PlateShow 371 |
| D13 | 65 | PlateShow: +9 = 7 | Area65_PlateShow 2029 |
| D14 | 65 | PlateGrow: step 0x1000 | Area65_PlateGrow 4000 |
| D15 | 65 | PlateHold: Field_Request 4 | Area65_PlateHold 361 |
| D16 | 65 | PlateShrink: released on 4 | Area65_PlateShrink 286 |
| D17 | 65 | HudRun: the other entry | Area65_HudRun 4000 |
| D18 | 65 | HudFrame: the box before the frame | Area65_HudFrame 4000 |
| D19 | 65 | FrameStep: the neighbour entry | Area65_FrameStep 4000 |
| D20 | 65 | FrameSlideIn: above 0x10 | Area65_FrameSlideIn 393 |
| D21 | 65 | FrameHold: mode byte 3 | Area65_FrameHold 668 |
| D22 | 65 | FrameSlideOut: below -0x30 | Area65_FrameSlideOut 70 |
| D23 | 65 | BoxStep: the neighbour entry | Area65_BoxStep 4000 |
| D24 | 65 | BoxSlideIn: below 0xC8 | Area65_BoxSlideIn 145 |
| D25 | 65 | BoxHold: 0x59 frames | Area65_BoxHold 232 |
| D26 | 65 | BoxSlideOut: above 0xF0 | Area65_BoxSlideOut 358 |
| D27 | 65 | DrawFrame: party set 0xD | Area65_DrawFrame 136 |
| D28 | 65 | DrawFrame: the second key over seven | Area65_DrawFrame 1478 |
| D29 | 65 | DrawSprite: CLUT 0x7B81 | Area65_DrawSprite 4000 |
| D30 | 65 | DrawHud: the cap at x + 0x7F | Area65_DrawHud 2945 |
| D31 | 65 | DrawHud: area 16's label cell 0x803588 | Area65_DrawHud 2945 |
| D32 | 65 | Record8Run: the next entry | Area65_Record8Run 4000 |
| D33 | 65 | Record8Place: the first nudge sar 12 | Area65_Record8Place 1832 |
| D34 | 65 | Record8Place: bank 0x47 | Area65_Record8Place 4000 |
| D35 | 65 | Record8Move: +0xB = 2 | Area65_Record8Move 664 |
| D36 | 65 | Record8Move: released on bit 6 | Area65_Record8Move 1346 |
| D37 | 65 | Record8Move: +0x38 += +0x14 | Area65_Record8Move 2696 |
| D38 | 65 | Record8Move: no script tick | Area65_Record8Move 2696 |
| D39 | 65 | Record8Move: +0xB set whatever bit 7 | Area65_Record8Move 697 |
| D40 | 65 | Record4Run: the other entry | Area65_Record4Run 4000 |
| D41 | 65 | Record4MarkCell: released on 8 | Area65_Record4MarkCell 1127 |
| D42 | 65 | Record4MarkCell: the cell 0xA1 | Area65_Record4MarkCell 2757 |
| D43 | 65 | DrawDrift: CLUT 0x78CC | Area65_DrawDrift 916 |
| D44 | 65 | DrawDrift: commit 0x44 | Area65_DrawDrift 1724 |
| G1 | 67 | ChoiceMark6: on choice 4 | Area67_ChoiceMark6 542 |
| G2 | 67 | ChoiceMark6: table B | Area67_ChoiceMark6 3873 |
| G3 | 67 | ChoiceMark6: 7 | Area67_ChoiceMark6 269 |
| G4 | 67 | ChoiceMessage: the next message | Area67_ChoiceMessage 3951 |
| G5 | 67 | ChoiceMessage: the choice zero-extended | Area67_ChoiceMark6 1213; Area67_ChoiceMessage 1230; Area67_ChoiceMark5 1192 |
| G6 | 67 | ChoiceMark5: choices 0..2 | Area67_ChoiceMark5 257 |
| G7 | 67 | ChoiceMark5: from -1 | Area67_ChoiceMark5 229 |
| G8 | 67 | ChoiceMark5: table A | Area67_ChoiceMark5 3919 |
| G9 | 67 | SetBit24: 0x20 | Area67_SetBit24 2978 |
| G10 | 67 | Object0XUp: 0x801 | Area67_Object0XUp 4000 |
| G11 | 67 | SpawnEffect2D: kind 0x2E | Area67_SpawnEffect2D 3583 |
| G12 | 67 | SpawnEffect2D: +0x38 from +0x3C | Area67_SpawnEffect2D 3583 |
| G13 | 67 | SpawnEffect2D: +0 = 3 | Area67_SpawnEffect2D 3583 |
| G14 | 67 | MemberSpawnAt: kind 2 | Area67_Member0SpawnAt 4000; Area67_Member1SpawnAt 4000; Area67_Member2SpawnAt 4000 |
| G15 | 67 | MemberSpawnAt: y and z swapped | Area67_Member0SpawnAt 4000; Area67_Member1SpawnAt 4000; Area67_Member2SpawnAt 4000 |
| G16 | 67 | MemberSpawnAt: 0xFF stored | Area67_Member0SpawnAt 778; Area67_Member1SpawnAt 810; Area67_Member2SpawnAt 762 |
| G17 | 67 | MemberSpawnAt: table B | Area67_Member0SpawnAt 2452; Area67_Member1SpawnAt 2440; Area67_Member2SpawnAt 2505 |
| G18 | 67 | MemberSpawnAt: Sprite_Current not set | Area67_Member0SpawnAt 3185; Area67_Member1SpawnAt 3215; Area67_Member2SpawnAt 3206 |
| G19 | 67 | Member0SpawnAt: member 1 | Area67_Member0SpawnAt 4000 |
| G20 | 67 | Member1SpawnAt: member 2 | Area67_Member1SpawnAt 4000 |
| G21 | 67 | Member2SpawnAt: member 0 | Area67_Member2SpawnAt 4000 |
| G22 | 67 | MemberSpawn: z from +0x32 | Area67_Member0Spawn3 4000; Area67_Member1Spawn3 4000; Area67_Member2Spawn3 4000; Area67_Member0Spawn1 4000; Area67_Member1Spawn1 4000; Area67_Member2Spawn1 4000; Area67_LeaderSpawn4 4000 |
| G23 | 67 | MemberSpawn: Sprite_Current not set | Area67_Member0Spawn3 3185; Area67_Member1Spawn3 3208; Area67_Member2Spawn3 3225; Area67_Member0Spawn1 3250; Area67_Member1Spawn1 3215; Area67_Member2Spawn1 3202; Area67_LeaderSpawn4 3144 |
| G24 | 67 | MemberSpawn: 0xFE not stored | Area67_Member0Spawn3 835; Area67_Member1Spawn3 742; Area67_Member2Spawn3 775; Area67_Member0Spawn1 868; Area67_Member1Spawn1 824; Area67_Member2Spawn1 800; Area67_LeaderSpawn4 835 |
| G25 | 67 | Member0Spawn3: kind 2 | Area67_Member0Spawn3 4000 |
| G26 | 67 | Member1Spawn3: member 2 | Area67_Member1Spawn3 4000 |
| G27 | 67 | Member2Spawn3: table B | Area67_Member2Spawn3 2485 |
| G28 | 67 | Member0Spawn1: table A | Area67_Member0Spawn1 2440 |
| G29 | 67 | Member1Spawn1: kind 3 | Area67_Member1Spawn1 4000 |
| G30 | 67 | Member2Spawn1: member 0 | Area67_Member2Spawn1 4000 |
| G31 | 67 | Object0XDown: - 0x7FF | Area67_Object0XDown 4000 |
| G32 | 67 | Object1XUp: object 1's z | Area67_Object1XUp 4000 |
| G33 | 67 | Object1XDown: + 0x800 | Area67_Object1XDown 4000 |
| G34 | 67 | MusicFade10: 11 frames | Area67_MusicFade10 4000 |
| G35 | 67 | LeaderSpawn4: kind 3 | Area67_LeaderSpawn4 4000 |
| G36 | 67 | LeaderSpawn4: table B | Area67_LeaderSpawn4 3803 |
| G37 | 67 | ResetOn6: on 7 | Area67_ResetOn6 1527 |
| G38 | 67 | ResetOn6: Var7 5 | Area67_ResetOn6 1111 |
| G39 | 67 | ResetOn6: 0x90384B kept | Area67_ResetOn6 1110 |
| G40 | 67 | ResetOn6: the bit cleared only on 6 | Area67_ResetOn6 1411 |
| G41 | 67 | ResetOn6: 0x8034E5 kept | Area67_ResetOn6 1108 |
| G42 | 67 | ResetOn6: bit 1 cleared | Area67_ResetOn6 2973 |
| G43 | 67 | ClearCell: row 0xC | Area67_ClearCell 4000 |
| G44 | 67 | ClearCell: value 1 | Area67_ClearCell 4000 |
| G45 | 67 | Trigger31: kind 0x2D | Area67_Trigger31 4000 |
| G46 | 67 | Trigger31: argument 0 | Area67_Trigger31 4000 |
| G47 | 67 | Trigger31: al 1 | Area67_Trigger31 4000 |
| G48 | 67 | Trigger31: state 1 | Area67_Trigger31 4000 |
