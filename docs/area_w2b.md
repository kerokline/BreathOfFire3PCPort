# World 2, areas 85..88: a CLUT shift, three floor switches, the world map's fifth and sixth copies

**Status:** IN PROGRESS (2026-09-28) - 66 functions ours
(`src/game/area_w2b.cpp`, shadow name `area_w2b`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)), one `Run` per area:
0 mismatches in 272,000 rounds; 147 controls planted, 146 refused by a count and one by a fault with its near variant refused by a count (section 8). Fuzz only: no recorded route reaches any of the 66 (section 7).
No divergence.

Group AR2B of round ten's fourth wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) section 12): the
band `0x40F720..0x411F10`, whole areas as `tools/area_rows.py --groups` cut
them. What each area *is* in the story is not read here: the names come
from what the code does.

## 1. The band, the areas, the function count

`analysis/area_funcs.tsv` (group `AR2B`) and the tool's `--unit AREA085..088
--clones` list 68 starts, two ours: `WorldMap_PinSprite` `0x4112A0` (round
seven's) and `WorldMap_FrameWait` `0x411310` (round eight's), both in area
88's block and shared by the eleven world maps. **All 66 others are
functions and all are taken: no start dropped, none added.** Every one was
read to its last instruction with capstone (the scratch `adis.py`); every
gap between them is `nop` padding. The tool's clone rows match the reading
call site for call site; no function in the band has a jump table.

| Area | Descriptor | Roots | Block | Fns |
|--:|---|---|---|--:|
| 85 | `0x6117A8` | `Area85_Handlers` (10), init; `0x40F9E0` also called by engine code | `0x40F720..0x40FA83` | 12 |
| 86 | `0x611C38` (two handlers `0x55B8B0` / `0x55B960`, chapter code outside the band) | cell hook (`Area_CellHooks`), init; and areas 65's and 87's shared init by address | `0x40FA90..0x40FC53` | 4 |
| 87 | `0x612488` (only a choice table, `+0x34` -> `0x41D270`, outside the band, and the init `0x40FC40`) | `WorldMap_Records` record 4 (`+0`, `+4`, `+8`, `+0xC`, `+0x10`), `WorldMap_FieldHooks` entry 4, six state tables in its data block | `0x40FC60..0x410D81` | 25 |
| 88 | `0x612E50` (only a choice table, `+0x34` -> `0x41D270`) | record 5, field hook entry 5, six state tables | `0x410D90..0x411F01` | 25 (and the two ours) |

**Shared bodies** (keyed by address, taken once):

| PC | Name | Reached by |
|---|---|---|
| `0x40F990` | `Area85_ReleaseSlots` | handler 7 of area 85 and handler 3 of area 198 (`0x649C9C`) |
| `0x40FC40` | `Area87_Init` | the init of areas 65 (`0x6046D0`, group AR1E's area) and 87; in area 86's block by address |

**Inbound from outside the band:** engine code at `0x478649` calls
`Area85_ClutShift` `0x40F9E0` (an effect state `0x478640`, reached through
the table `0x6545B8`, with its record's dword `+0x10` as the delta);
area 198's descriptor names `Area85_ReleaseSlots`; area 65's names
`Area87_Init`. For the rebinding pass.

**The tool's two known gaps** (round10 doc sections 7 and 10), looked for:
no start without padding or a `ret` before it; no tail kind armed in the
band at all (no `0x9039F3` store). **A third kind of false positive**,
new: the tool marks `0x40FDE0` (area 87's `PlateRun`) "shared" by areas
6, 9, 122 and 123, and `0x40FF20` (`PlateShow`) by areas 24 and 67, and
the plate states behind them shared with them - the dwords it read
(`0x5DF800`, `0x5DFA30`, `0x5E15B0`, `0x6249C0`, `0x5E96EC`, `0x605B0C`, ...)
are pairs of s16 coordinates (`0xFDE0, 0x0040`) in those areas' data, not
code pointers. Every one of area 87's 25 is area 87's alone.

## 2. Area 85

| PC | Name | Size | Root | PSX twin | What |
|---|---|--:|---|---|---|
| `0x40F720` | `Area85_Kind2Walk` | `0x55` | handler 0 | `0x801F373C` | `MoveScript_Object[7]` = (`Sprite_Current +0x34` - `0x160000`) `sar 15` (a byte: the kind-2 object's step count); `MoveCmd_MoveKind2(7)`; then (both pointers read again) `Sprite_Current +0x14 = 0`, `MoveScript_FAWord = 0`, `MoveScript_Object +0 |= 0x40`, `Field_Kind2Z` and `Sprite_Current +0x38` = `0x3E0000` |
| `0x40F780` | `Area85_FollowMember2` | `0xA8` | handler 1 | `0x801F37C0` | in run 6 step `0xC` (`MoveScript_Var7`, `0x8034E5`): `Sprite_Current +0` bit 6 cleared, `Field_MemberSprite(2, 2)`, `Sprite_SetAnimation(1)`. Otherwise the party record(s) whose `+0x89` is 2 give their `+0x34` / `+0x38` / `+0x3C` to `Sprite_Current`, and the script word `+0xA` - 2 (the op runs again next frame) |
| `0x40F830` | `Area85_SetType7` | `0x17` | handler 2 | `0x801F38D0` | `Sprite_Current +6 = 7`, `Field_ActiveMember +0x9E = 0` |
| `0x40F850` | `Area85_SpawnEffect47A` | `0x62` | handler 3 | `0x801F38F0` | `Sprite_Current +0xB` = `Effect_FindFree()`; none: the script word - 2 (it waits for a slot); else the record `+0 = 1`, kind `+5 = 0x47`, `+0xB = 1`, `Sound_PlayEffect(0x200)` |
| `0x40F8C0` | `Area85_SpawnEffect47B` | `0x62` | handler 4 | `0x801F39F4` | the same with `+0xB = 0` |
| `0x40F930` | `Area85_SetUpObject` | `0x2F` | handler 5 | `0x801F3AF8` | `+0x24` bit 0 cleared, `Sprite_SetAnimationBank(0x184)`, `Sprite_SetAnimation(0)`, `+0x2A = 1`, `+0 |= 0x10` |
| `0x40F960` | `Area85_StartSlotScript` | `0x27` | handler 6 | `0x801F3B5C` | `0x454A80(Sprite_Current)` (the object's `Field_Slots` scripts released), `0x455290(Sprite_Current, 0x611814)` (a slot for the movement script at `0x611814`, its answer not read), `Sprite_SetAnimation(0)` |
| `0x40F990` | `Area85_ReleaseSlots` | `0xD` | handler 7 (and area 198's 3) | `0x801F3BA4` (198: `0x801F2FD0`) | `0x454A80(Sprite_Current)` |
| `0x40F9A0` | `Area85_Raise18` | `0xB` | handler 8 | `0x801F3BCC` | the word `+0x3E` += `0x18` |
| `0x40F9B0` | `Area85_Sound201` | `0x15` | handler 9 | `0x801F3BEC` | `Sound_PlayEffect(0x201)` unless `Field_Request` is 5 |
| `0x40F9D0` | `Area85_Init` | `0x9` | init | `0x801F3C1C` | `Area85_ClutShift(-8)` |
| `0x40F9E0` | `Area85_ClutShift` | `0xA4` | called (and from `0x478649`) | | for the 256 words of CLUT row 6 as loaded (`0x80C180`, `Gfx_ClutStripSource + 0xC00`): each 5-bit channel that is not 0 moved by `delta` and held to 0..`0x1F` (signed 32-bit), bit 15 kept, to the live strip's row 6 (`0x810180`, `Gfx_ClutStrip + 0xC00`); `Gfx_ClutStripDirty = 1` |

So area 85's entry darkens one CLUT row by 8 steps a channel (a channel at
0 stays 0), and an engine effect state (`0x478640`, whose delta counts
back toward 0 one step every 16 frames by `Frame_Counter & 0xF`, then
`Effect_Release`) brightens it back: a fade of whatever uses row 6, read
off the code. Tables: `Area85_Handlers` `0x61177C` (10, then 0). The
movement script `0x611814` handed to `0x455290` is data, not described.

## 3. Area 86: three floor switches

| PC | Name | Size | Root | What |
|---|---|--:|---|---|
| `0x40FA90` | `Area86_SwitchHook` | `0xDB` | cell hook | `(x, z)`: the first of `Area86_Switches`' three `(x, z, facing, flag)` records whose x, z are the cell bytes and facing the leader's `+8`; none al 0. Then for every cell of that switch's rectangle in `Area86_Rects` (`x0 <= x < x1`, `z0 <= z < z1`) `Area86_MemberNear(x, z)`: a member there (answer 0..0x7F) is al 0. Story flag `0x1C` set is al 0. Else `0x57C160` toggles the switch's story flag, `Sound_PlayEffect(0x200)`, `0x469FE0(0xF)` (an effect of kind 4, which also sets flag `0x1C`: one switch at a time), al 1 |
| `0x40FB70` | `Area86_MemberNear` | `0x92` | called | the first party record 0 .. `Field_MemberCount` - 1 with `+0` not 0 whose position a step ahead (`+0xC * +9 + +0x34`, and `+0x10` / `+0x38`) is within `((+0x70) + 2) << 15` of the cell `(x << 16, z << 16)` in both axes (absolute values by `cdq / xor / sub`, strict): its index; none `0xFF` |
| `0x40FC10` | `Area86_Init` | `0x23` | init (PSX `0x801F3604`) | while the s8 chapter byte `Cond_ByteFA` is below `0xD`: `AreaMap_SetByte(0xB, 9, 0x50)` and `(0xB, 0xA, 0x50)` |
| `0x40FC40` | `Area87_Init` | `0x14` | init of areas 65 and 87 (PSX `0x801F2C38`) | when the party came from area `0x3C` (the word `0x802290`, [`mode-flow.md`](mode-flow.md)): `Field_ScriptFlags` bit 13 cleared |

So each switch flips its own story flag (the three are consecutive) only
when no party member stands in that switch's strip of cells, and not while
the kind-4 effect of the last one runs. What the flags open is the
scripts' to say. The PC descriptor has two handlers where the PSX
descriptor has none (`area_pairs.json`'s one disagreeing area besides 75,
[`area-rows.md`](area-rows.md) section 2): they are chapter code
(`0x55B8B0`, `0x55B960`), outside this band. Tables: `Area86_Switches`
`0x611C7C` (3 x 4), `Area86_Rects` `0x611C88` (3 x 4).

## 4. Areas 87 and 88: the world map's fifth and sixth copies

Areas 87 and 88 are `WorldMap_Records` records 4 and 5 (area bytes `0x57`,
`0x58`) and `WorldMap_FieldHooks` entries 4 and 5
([`worldmap_area.md`](worldmap_area.md) section 5). **Their code is area
45's** ([`area_w1b.md`](area_w1b.md) section 5, itself area 16's,
[`area_w0b.md`](area_w0b.md) section 2) **instruction for instruction over
their own tables**: a capstone compare of the 25 pairs each (the scratch
`cmp.py`: instruction by instruction, addresses inside a function made
relative) finds only jump targets, calls to each copy's own functions and
the table operands differing, **and these operands more**:

| Function | Area 45 | Area 87 | Area 88 |
|---|---|---|---|
| `PlateStart` | `push 0x3B` (bank; 0x4E bytes) | `push 0x156` (an imm32: 0x51 bytes) | `push 0x157` (0x51 bytes) |
| `DrawHud` | label offset from the dword `0x803584` | `0x803580` | `0x803580` |
| `PlaceMessage` | name sets of 5 bytes, 4 text rows | the same | **name sets of 12 bytes (`lea ecx, [ebp + ebp*2]; lea edi, [ecx*4 + 0x612F75]`), eleven text rows to `0x904E40`** - 0x178 bytes, 4 longer |

Area 16's `DrawHud` reads `0x803588`. So the "two constants" of the
copies are three here: the plate's animation bank, the region label's
cell, and area 88's place hook, which names up to eleven items a place
where the others name four. The rest is described once, in the three
docs above; ours is one body of code over a `WorldMapTables` per copy
(`area_w2b_callees.h`), with a named entry per address:

| Function | Area 45 | Area 87 | Area 88 | Size |
|---|---|---|---|--:|
| `PlaceMessage` (field hook) | `0x407B40` | `0x40FC60` | `0x410D90` | `0x174` / `0x174` / `0x178` |
| `PlateRun` (record `+0`) | `0x407CC0` | `0x40FDE0` | `0x410F10` | `0xD6` |
| `PlateStart` / `Show` / `Grow` / `Hold` / `Shrink` (plate states 0..4) | `0x407DA0..` | `0x40FEC0`, `0x40FF20`, `0x410070`, `0x4100C0`, `0x410120` | `0x410FF0`, `0x411050`, `0x4111A0`, `0x4111F0`, `0x411250` | `0x51`, `0x142`, `0x41`, `0x58`, `0x50` |
| `HudRun` (record `+0xC`) / `HudFrame` | `0x408040` / `0x408060` | `0x410170` / `0x410190` | `0x4112C0` / `0x4112E0` | `0x12`, `0xA` |
| `FrameStep` / `SlideIn` / `Hold` / `SlideOut` | `0x408070..` | `0x4101A0`, `0x4101C0`, `0x4101F0`, `0x410220` | `0x4112F0`, `0x411340`, `0x411370`, `0x4113A0` | `0x12`, `0x21`, `0x28`, `0x41` |
| `BoxStep` / `SlideIn` / `Hold` / `SlideOut` | `0x408140..` | `0x410270`, `0x410290`, `0x410300`, `0x410370` | `0x4113F0`, `0x411410`, `0x411480`, `0x4114F0` | `0x12`, `0x62`, `0x6E`, `0x57` |
| `DrawFrame` / `DrawSprite` / `DrawHud` | `0x4082A0` / `0x408470` / `0x4086D0` | `0x4103D0` / `0x4105A0` / `0x410660` | `0x411550` / `0x411720` / `0x4117E0` | `0x1C5`, `0xBC`, `0x58` |
| `Record8Run` (record `+8`) / `Record8Place` | `0x408730` / `0x408750` | `0x4106C0` / `0x4106E0` | `0x411840` / `0x411860` | `0x12`, `0x154` |
| `Record4Run` (record `+4`) / `Record4MarkCell` | `0x4088B0` / `0x4088D0` | `0x410840` / `0x410860` | `0x4119C0` / `0x4119E0` | `0x12`, `0xB5` |
| `DrawDrift` (record `+0x10`) | `0x4089A0` | `0x410920` | `0x411AA0` | `0x462` |

Names are `Area87_*` / `Area88_*` as the merged copies'. Both call
`WorldMap_DrawNeedle` `0x408530` (round seven's) and `WorldMap_PinSprite`;
their tables' state 0 entries are round eight's shared `WorldMapHud_Start`,
`WorldMap_FrameWait`, `WorldMapHud_BoxWait`; the record-4 table's state 1
is AR1B's `Area45_Record4Tick`; the record-8 table's first and third are
`0x4253C0` and `0x40C490` (other groups', not called here).

**The tables** (`[[data]]`, each in the area's data block; the tool moves
their code pointers to the area before the next descriptor, as for area
45):

| Table | Area 87 | Area 88 |
|---|---|---|
| `PlateAnims` (u16 place, u8 animation, u8) | `0x611C98` (10) | `0x612700` (7) |
| `Cells` (x, z, -, id) | `0x611CC0` (3, then a zero record) | `0x61271C` (3, then the descriptor's data) |
| `PlaceMessages` (rows of 0x20) | `0x6124CC` (10) | `0x612E94` (7) |
| `NameSets` | `0x61260C` (3 x (id, 4 items)) | `0x612F74` (3 x (id, 11 items)) |
| `PlateStates` / `HudStates` / `FrameStates` / `BoxStates` | `0x61261C` / `0x612630` / `0x612638` / `0x612648` | `0x612F98` / `0x612FAC` / `0x612FB4` / `0x612FC4` |
| `Sprites` (22 x 4) / `Buttons` (6, read to 8) | `0x612658` / `0x6126B0` | `0x612FD4` / `0x61302C` |
| `Record8States` / `Directions` / `Record8Anims` / `Record4States` | `0x6126C8` / `0x6126D4` / `0x6126E4` / `0x6126EC` | `0x613044` / `0x613050` / `0x613060` / `0x613068` |
| `DriftUV` (12) | `0x6126F4` | `0x613070` |

The cell tables are three records each, the "searched with no bound" of
area 45 in the flesh: the place hook's search for the leader's cell runs
on into the descriptor's data (area 88) or stops at area 87's zero record
only for cell (0, 0).

## 5. The fuzz

`BOF3X_SHADOW=area_w2b` (`src/game/area_w2b_fuzz.cpp`): four `Run` calls
under the one shadow name, `Group::area` each area's number, the real
descriptors and tables in place. `BOF3X_AR2B_AREA=n` runs one area's
group alone (the controls script's shortcut).

- **Area 85** (4,000 rounds a function): regions `MoveScript_Object`,
  `Field_ActiveMember` (both seeded to a party record or one of the first
  four field objects), `Effect_Objects`' twenty records, CLUT row 6 as
  loaded and live. Listed: `MoveCmd_MoveKind2`, `Effect_FindFree` (`kByte
  0xFF..0x13`), `0x454A80`, `0x455290` (`kByte 0xFF..0x07`) and
  `Area85_ClutShift` (for the init). Seeds: `+0x34` at `0x160000` and a
  half cell either side and `0x80000000`; run 6 / step `0xC` and their
  neighbours; each party record's `+0x89` 2 or not; `Field_MemberCount`
  0..4; `Field_Request` 5 and neighbours; CLUT words with every channel at
  0, 1, 8, 9, `0x17`, `0x18`, `0x1E`, `0x1F` and bit 15 either way; the
  shift's delta -8, 0, +-1, 8, `0x1E`, `0x1F`, `0x20`, `-0x1F`, `-0x20`
  and the 32-bit extremes. `disturb` moves `MoveScript_Object` (handler 0
  reads it again after its call) and `Field_ActiveMember`.
- **Area 86** (6,000): the cell hook's `(x, z)` a switch's cell bytes
  (two rounds in three, with bits above, one off, or anything), the
  leader's facing the switch's two rounds in three; `Area86_MemberNear`
  stood in by a recorder answering "none" (`0xFF`, `0x80`, `0xFE`) eleven
  calls in twelve and a member (0..2, `0x7F`) in the twelfth - every cell
  of a rectangle must be clear for the switch to flip; `0x57C160`
  (index masked to a byte: pushed with `Flags_Test`'s answer above it),
  `0x469FE0`. `MemberNear` itself: its cell drawn by the seed (an `args`
  hook's writes are lost), each party record planted at the reach - 1,
  the reach, + 1 on either side in each axis, with one axis sometimes far
  off, `+0x70` at 0, 1, `0xFE`, `0xFF`, `+9` 0..2 and `0xFF`, `+0` 0 or
  not, the count 0..4 and `0x80`. Area 86's init with `Cond_ByteFA` `0xC`,
  `0xD`, `0xE` and the sign's edges; `Area87_Init` with `0x802290` `0x3C`,
  its neighbours, `0x13C` and `0x803C` (a region of the group's).
- **Areas 87 and 88** (4,000 each): area 45's group
  ([`area_w1b.md`](area_w1b.md) section 8, itself area 16's) over each
  copy's tables - the six state tables as `DataTable`s, the packet buffer,
  items and names of the fuzz's, the copy's plate, cell, place, name,
  direction and drift tables as regions (only the tables: the
  descriptor's data between the cell records and the place rows is left
  alone), put back to the exe's bytes two rounds in three; `settle`
  keeping `+1` inside the plate table and re-planting the leader's cell in
  the last cell record (area 87's zero record, area 88's third); the text
  rows' region `0x80` bytes for area 87, `0x160` for area 88's eleven.

**Result (in this worktree):** 0 mismatches in every run;
rounds / calls to the stand-ins in the `'*'` run: area 85 48,000 / 51,325;
86 24,000 / 9,383; 87 100,000 / 520,715; 88 100,000 / 536,833 (the four
runs share the harness's random stream, so a change to one group moves the
next groups' counts). Every state-table entry reached (the coverage
lines' phase counts: each copy's plate states 772..837, frame and box
states about 1,000 each and the holds about 5,000, record-8 states about
1,330, record-4 about 2,000); area 86's switch toggles 121 times, its flag
test reached 365 times. `BOF3X_SHADOW='*'`: exit 0, 409 self-test lines, no
mismatch or Fatal, `inject: 4620 ours` (all 66 of this group's injected;
4,554 before).

## 6. Latent defects (described, not fixed; the fuzz keeps inside them)

1. `Area85_FollowMember2` and `Area86_MemberNear` walk party records to
   `Field_MemberCount` unchecked against the three `ObjTrio` records (a
   count above 3 reads the bytes after them).
2. `Area85_SpawnEffect47A` / `B` index `Effect_Objects` by the slot they
   stored in `+0xB` unchecked (`Effect_FindFree` answers 0..19 or `0xFF`;
   ours aborts past twenty).
3. Areas 87 and 88 have area 45's: the unbounded cell and plate searches
   (D74's shape; area 88's cell table has no terminator, so a leader's
   cell in no record reads on through the descriptor's data), the name set
   3 reading the plate state table's bytes, the unchecked record and table
   indexes, the unchecked `.data` dispatches (ours aborts past each of the
   six tables, as AR0C's and AR1B's do).

**Numbered 2026-09-28** ([`round-10-cleanup.md`](round-10-cleanup.md) item 2):
the defects above are D133 (the dispatchers), D134 (tables back to back), D135
(`Effect_FindFree`'s none as a slot), D136 (reads and writes by an unchecked
byte or count), D143 (the world-map copies' searches) in
[`known-defects.md`](known-defects.md).

## 7. What reaches it, calls across groups

- **Reach - no recorded route reaches any of the 66.** The world-map
  route's traces (`analysis/calltrace/recipe_worldmap/bof3x.callcounts.tsv`,
  `hidden_worldmap/bof3x.calltrace.tsv`, `analysis/hidden_reached_worldmap.json`)
  hold two addresses of this band, both already ours and both shared by
  the eleven maps: `WorldMap_PinSprite` `0x4112A0` (259 calls, from area
  33's plate states `0x404035` / `0x404085` / `0x4040E5`) and
  `WorldMap_FrameWait` `0x411310` (hidden, from `0x404155`, area 33's HUD
  frame). The route plays area 33's copy; none of area 88's own code runs
  on it, whatever the tool's "live" column says for the two shared ones.
  The coordinator's world-map A/B can only show that nothing moved.
- **Cross-group raw-address calls:** `0x57C160` (the flag toggle) and
  `0x469FE0` (an effect of kind 4) - group SX2's this wave; `0x454A80` and
  `0x455290` (`Field_Slots` release and start, engine code nobody owns;
  read 2026-09-28, in `area_w2b_callees.h`). Named and ours:
  `MoveCmd_MoveKind2`, `Field_MemberSprite`, `Effect_FindFree`,
  `AreaMap_SetByte`, `WorldMap_DrawNeedle`, `WorldMap_PinSprite`,
  `WorldMap_RecordIndex`, `Item_NamePtr`, the standard set.
- **Inbound:** `0x478649` (engine) into `Area85_ClutShift`; area 198's
  handler 3 is `Area85_ReleaseSlots`; area 65's init is `Area87_Init`.
- `analysis/calltrace/entries_logic.txt`: 62 lines appended; `004103D0
  1C5`, `004105A0 BC`, `00411550 1C5`, `00411720 BC` were there. Host
  lines with larger extents cover `0x40F720..0x40F9D0` (`0040F140 899`),
  `0x40F9E0` (`18B`), `0x40FB70` (`62A`), `0x4101A0` (`227`), `0x410660`
  (`C40`), `0x4112F0` (`20`, mine `12`), `0x4117E0` (`95A`): the smaller
  extents are appended beside them.

## 8. Controls

Planted one at a time in `area_w2b.cpp` (and, for the table constants,
`area_w2b_callees.h`) by a script (the scratch `controls.py`, not
committed): each anchored on a string the file holds once; plant, rebuild
(checking `area_w2b.cpp` recompiled), run the area's group alone
(`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=area_w2b BOF3X_AR2B_AREA=n`), restore;
after the last, a rebuild and a clean full run (exit 0, 0 mismatches in all
four runs). The world-map body, shared by areas 87 and 88, is planted
under area 87; each copy's own entries, table constants and wrappers under
its own area. **147 planted, 146 refused by a count** (exit 3), at least
one per function; **1 refused by a fault** (C62: area 87's plate table
moved on an entry - the unbounded search then reads on through `.data`,
as the original's would with a place in no entry), its near variant C62b
refused by a count. None stood.

A first run of the set (before the fuzz's last strengthening) stood on
two: B4 (the cell hook's z masked to 7 bits - every switch's z byte is
below `0x80` and no argument carried bit 7 alone; the arguments now flip it
one draw in eight) and C62 (the fuzz planted the place and built its
regions from the same `kWm87` constants ours reads, so a wrong table
address moved both; the fuzz now has its own literal table addresses,
`kFz87` / `kFz88`). C6 (the place hook's x not read again after the call)
and D16 were refused by faults on that run (a stale cell word matched no
record and the search ran off); the settle now plants the stale pairs as
records of another name set, and both are refused by a count. The table is
the second run (in this worktree; rounds of a function's 4,000, 6,000 in
area 86).

| # | Area | planted | refused in rounds |
|---|--:|---|---|
| A1 | 85 | Kind2Walk: step count >> 14 | Area85_Kind2Walk 2962 |
| A2 | 85 | Kind2Walk: direction 6 | Area85_Kind2Walk 4000 |
| A3 | 85 | Kind2Walk: +0x18 zeroed | Area85_Kind2Walk 4000 |
| A4 | 85 | Kind2Walk: bit 5 | Area85_Kind2Walk 3020 |
| A5 | 85 | Kind2Walk: Kind2Z 0x3E0001 | Area85_Kind2Walk 4000 |
| A6 | 85 | Kind2Walk: MoveScript_Object not read again after the call | Area85_Kind2Walk 41 |
| A7 | 85 | Follow: run 7 | Area85_FollowMember2 427 |
| A8 | 85 | Follow: step 0xD | Area85_FollowMember2 404 |
| A9 | 85 | Follow: slot 1 | Area85_FollowMember2 273 |
| A10 | 85 | Follow: member 1 | Area85_FollowMember2 1942 |
| A11 | 85 | Follow: +0x3C not copied | Area85_FollowMember2 1650 |
| A12 | 85 | Follow: script word - 3 | Area85_FollowMember2 3727 |
| A13 | 85 | Follow: bit 7 cleared | Area85_FollowMember2 215 |
| A14 | 85 | Follow: one record more | Area85_FollowMember2 680 |
| A15 | 85 | SetType7: 6 | Area85_SetType7 4000 |
| A16 | 85 | SetType7: +0x9F | Area85_SetType7 4000 |
| A17 | 85 | Spawn: kind 0x48 | Area85_SpawnEffect47A 3797; Area85_SpawnEffect47B 3780 |
| A18 | 85 | SpawnA: +0xB 2 | Area85_SpawnEffect47A 3797 |
| A19 | 85 | SpawnB: +0xB 1 | Area85_SpawnEffect47B 3780 |
| A20 | 85 | Spawn: sound 0x201 | Area85_SpawnEffect47A 3797; Area85_SpawnEffect47B 3780 |
| A21 | 85 | Spawn: none, script word - 1 | Area85_SpawnEffect47A 203; Area85_SpawnEffect47B 220 |
| A22 | 85 | SetUpObject: bank 0x185 | Area85_SetUpObject 4000 |
| A23 | 85 | SetUpObject: +0x2A 2 | Area85_SetUpObject 4000 |
| A24 | 85 | SetUpObject: bit 1 | Area85_SetUpObject 2974 |
| A25 | 85 | StartSlot: script + 4 | Area85_StartSlotScript 4000 |
| A26 | 85 | StartSlot: animation 1 | Area85_StartSlotScript 4000 |
| A27 | 85 | ReleaseSlots: the active member | Area85_ReleaseSlots 3410 |
| A28 | 85 | Raise18: 0x17 | Area85_Raise18 4000 |
| A29 | 85 | Sound201: request 4 | Area85_Sound201 1364 |
| A30 | 85 | Sound201: sound 0x202 | Area85_Sound201 3093 |
| A31 | 85 | Init: -7 | Area85_Init 4000 |
| A32 | 85 | Shift: 1 clamps to 0 | Area85_ClutShift 709 |
| A33 | 85 | Shift: clamp 0x1E | Area85_ClutShift 1767 |
| A34 | 85 | Shift: channel 0 moved too | Area85_ClutShift 1994 |
| A35 | 85 | Shift: bit 14 kept | Area85_ClutShift 4000 |
| A36 | 85 | Shift: dirty 2 | Area85_ClutShift 4000 |
| A37 | 85 | Shift: 255 words | Area85_ClutShift 4000 |
| A38 | 85 | Shift: g from bit 6 | Area85_ClutShift 2476 |
| B1 | 86 | SwitchHook: facing & 7 | Area86_SwitchHook 41 |
| B2 | 86 | SwitchHook: none answers 1 | Area86_SwitchHook 5600 |
| B3 | 86 | SwitchHook: two switches searched | Area86_SwitchHook 5600 |
| B4 | 86 | SwitchHook: z & 0x7F | Area86_SwitchHook 168 |
| B5 | 86 | SwitchHook: x1 inclusive | Area86_SwitchHook 187 |
| B6 | 86 | SwitchHook: z from z0 + 1 | Area86_SwitchHook 400 |
| B7 | 86 | SwitchHook: member 0 ignored | Area86_SwitchHook 63 |
| B8 | 86 | SwitchHook: the answer unsigned | Area86_SwitchHook 363 |
| B9 | 86 | SwitchHook: flag 0x1D | Area86_SwitchHook 187 |
| B10 | 86 | SwitchHook: the facing byte toggled | Area86_SwitchHook 63 |
| B11 | 86 | SwitchHook: effect 0xE | Area86_SwitchHook 63 |
| B12 | 86 | SwitchHook: al 2 | Area86_SwitchHook 63 |
| B13 | 86 | SwitchHook: (z, x) asked | Area86_SwitchHook 400 |
| B14 | 86 | MemberNear: +1 tested | Area86_MemberNear 315 |
| B15 | 86 | MemberNear: reach + 3 | Area86_MemberNear 1467 |
| B16 | 86 | MemberNear: x at the reach | Area86_MemberNear 310 |
| B17 | 86 | MemberNear: z at the reach | Area86_MemberNear 334 |
| B18 | 86 | MemberNear: steps from +8 | Area86_MemberNear 1631 |
| B19 | 86 | MemberNear: z from +0x3C | Area86_MemberNear 1635 |
| B20 | 86 | MemberNear: none 0xFE | Area86_MemberNear 4364 |
| B21 | 86 | MemberNear: x not absolute | Area86_MemberNear 333 |
| B22 | 86 | Init86: chapter 0xC | Area86_Init 556 |
| B23 | 86 | Init86: 0x51 | Area86_Init 3328 |
| B24 | 86 | Init86: x 0xA | Area86_Init 3328 |
| B25 | 86 | Init87: area 0x3D | Area87_Init 1442 |
| B26 | 86 | Init87: bit 14 | Area87_Init 1148 |
| B27 | 86 | Init87: the area a byte | Area87_Init 1003 |
| B28 | 86 | MemberNear: the count signed | Area86_MemberNear 192 |
| C1 | 87 | PlaceMessage: state 2 handled as 1 | Area87_PlaceMessage 777 |
| C2 | 87 | PlaceMessage: waits on 3 | Area87_PlaceMessage 265 |
| C3 | 87 | PlaceMessage: 0x9039F5 kept | Area87_PlaceMessage 511 |
| C4 | 87 | PlaceMessage: 0xA0 a place | Area87_PlaceMessage 292 |
| C5 | 87 | PlaceMessage: Cond_ByteFA zero-extended | Area87_PlaceMessage 63 |
| C6 | 87 | PlaceMessage: x not read again | Area87_PlaceMessage 5 |
| C9 | 87 | PlaceMessage: 0xFE ends | Area87_PlaceMessage 194 |
| C10 | 87 | PlaceMessage: unseen name | Area87_PlaceMessage 604 |
| C11 | 87 | PlaceMessage: key item 0x17 | Area87_PlaceMessage 141 |
| C12 | 87 | PlaceMessage: name + 0x37 | Area87_PlaceMessage 730 |
| C13 | 87 | PlaceMessage: set + 0x17 | Area87_PlaceMessage 789 |
| C14 | 87 | PlaceMessage: request 3 | Area87_PlaceMessage 991 |
| C15 | 87 | PlateRun: 0xA0 kind 3 | Area87_PlateRun 323 |
| C16 | 87 | PlateRun: bit 11 | Area87_PlateRun 1277 |
| C17 | 87 | PlateRun: the next state | Area87_PlateRun 4000 |
| C18 | 87 | PlateStart: bank + 1 | Area87_PlateStart 4000 |
| C19 | 87 | PlateStart: +0x29 4 | Area87_PlateStart 4000 |
| C20 | 87 | PlateShow: kind 2 animation 2 | Area87_PlateShow 352 |
| C21 | 87 | PlateShow: +9 7 | Area87_PlateShow 2029 |
| C22 | 87 | PlateShow: +0x44 | Area87_PlateShow 2033 |
| C23 | 87 | PlateGrow: step 0x1000 | Area87_PlateGrow 3999 |
| C24 | 87 | PlateHold: request 4 | Area87_PlateHold 346 |
| C25 | 87 | PlateHold: Game_Mode 2 holds | Area87_PlateHold 825 |
| C26 | 87 | PlateShrink: released on 4 | Area87_PlateShrink 294 |
| C27 | 87 | PlateShrink: back to 2 | Area87_PlateShrink 598 |
| C28 | 87 | HudRun87: the other entry | Area87_HudRun 4000 |
| C29 | 87 | HudFrame87: the box first | Area87_HudFrame 4000 |
| C30 | 87 | FrameStep87: the next entry | Area87_FrameStep 4000 |
| C31 | 87 | FrameSlideIn: above 0x10 | Area87_FrameSlideIn 389 |
| C32 | 87 | FrameHold: mode 3 | Area87_FrameHold 691 |
| C33 | 87 | FrameSlideOut: below -0x30 | Area87_FrameSlideOut 53 |
| C34 | 87 | BoxStep87: the next entry | Area87_BoxStep 4000 |
| C35 | 87 | BoxSlideIn: below 0xC8 | Area87_BoxSlideIn 119 |
| C36 | 87 | BoxHold: 0x59 frames | Area87_BoxHold 229 |
| C37 | 87 | BoxSlideOut: above 0xF0 | Area87_BoxSlideOut 344 |
| C38 | 87 | BoxLeaves: bit 9 | Area87_BoxSlideIn 1006; Area87_BoxHold 882 |
| C39 | 87 | DrawFrame: second key over seven | Area87_DrawFrame 1458 |
| C40 | 87 | DrawFrame: legend 3 y + 0x17 | Area87_DrawFrame 2533 |
| C41 | 87 | DrawFrame: party set & 0xFF | Area87_DrawFrame 51 |
| C42 | 87 | DrawSprite: CLUT 0x7B81 | Area87_DrawSprite 4000 |
| C43 | 87 | DrawSprite: semi by & 0x7F | Area87_DrawSprite 3 |
| C44 | 87 | DrawHud: cap at x + 0x7F | Area87_DrawHud 2931 |
| C45 | 87 | DrawHud: label & 0xFFF | Area87_DrawHud 2753 |
| C46 | 87 | Record8Place: bank 0x47 | Area87_Record8Place 4000 |
| C47 | 87 | Record8Place: first nudge sar 12 | Area87_Record8Place 1724 |
| C48 | 87 | Record8Place: up for +6 2 | Area87_Record8Place 1511 |
| C49 | 87 | Record4MarkCell: cell 0xA1 | Area87_Record4MarkCell 2707 |
| C50 | 87 | Record4MarkCell: released on 8 | Area87_Record4MarkCell 1155 |
| C51 | 87 | Record4MarkCell: bank 0x204 | Area87_Record4MarkCell 2707 |
| C52 | 87 | DrawDrift: wrap to -7 | Area87_DrawDrift 1674 |
| C53 | 87 | DrawDrift: x within 24 | Area87_DrawDrift 497 |
| C54 | 87 | DrawDrift: CLUT 0x78CC | Area87_DrawDrift 862 |
| C55 | 87 | DrawDrift: commit 0x44 | Area87_DrawDrift 1659 |
| C56 | 87 | DrawDrift: size from the u table | Area87_DrawDrift 861 |
| C57 | 87 | DrawDrift: +0x38 by b << 9 | Area87_DrawDrift 3032 |
| C58 | 87 | Record8Run87: the next entry | Area87_Record8Run 4000 |
| C59 | 87 | Record4Run87: the other entry | Area87_Record4Run 4000 |
| C60 | 87 | kWm87: bank 0x157 | Area87_PlateStart 4000 |
| C61 | 87 | kWm87: nine place rows | Area87_PlaceMessage 68 |
| C62 | 87 | kWm87: plate animations from the second | a fault (exit 0xC0000005): the search runs off `.data`; its variant C62b by a count |
| C62b | 87 | PlateShow: the animation from the pad byte | Area87_PlateShow 999 |
| C63 | 87 | kWm87: sprites from the second | Area87_DrawSprite 4000 |
| C64 | 87 | kWm87: drift size from the next | Area87_DrawDrift 772 |
| C65 | 87 | PlateGrow: no pin | Area87_PlateGrow 4000 |
| D1 | 88 | PlaceMessage88: sets of 5 | Area88_PlaceMessage 553 |
| D2 | 88 | PlaceMessage88: four rows | Area88_PlaceMessage 437 |
| D3 | 88 | PlateStart88: area 87's bank | Area88_PlateStart 4000 |
| D4 | 88 | DrawSprite88: area 87's sprites | Area88_DrawSprite 1452 |
| D5 | 88 | DrawDrift88: area 87's UV | Area88_DrawDrift 475 |
| D6 | 88 | HudRun88: the other entry | Area88_HudRun 4000 |
| D7 | 88 | Record4MarkCell88: area 87's cells | Area88_Record4MarkCell 2680 |
| D8 | 88 | FrameStep88: the next entry | Area88_FrameStep 4000 |
| D9 | 88 | Record8Run88: the next entry | Area88_Record8Run 4000 |
| D10 | 88 | Record4Run88: the other entry | Area88_Record4Run 4000 |
| D11 | 88 | BoxStep88: the next entry | Area88_BoxStep 4000 |
| D12 | 88 | HudFrame88: the box first | Area88_HudFrame 4000 |
| D13 | 88 | kWm88: bank 0x156 | Area88_PlateStart 4000 |
| D14 | 88 | kWm88: ten text rows | Area88_PlaceMessage 418 |
| D15 | 88 | kWm88: six place rows | Area88_PlaceMessage 60 |
| D16 | 88 | kWm88: directions from the second word | Area88_Record8Place 3967 |
| D17 | 88 | PlaceMessage88: area 87's tables | Area88_PlaceMessage 973 |
