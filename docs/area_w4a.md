# World 4, areas 152..155, 166..167: the world map's last copy, five choices, a flag-and-height area with its own tail kind

**Status:** IN PROGRESS (2026-09-28) - 48 functions ours
(`src/game/area_w4a.cpp`, shadow name `area_w4a`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)), one `Run` per area:
0 mismatches in 272,000 rounds (section 9, in this worktree); 224 controls
planted, 221 refused by a count and 3 by ours' abort with their near variants
refused by a count (section 10). Fuzz only: no recorded route reaches any of the 48 (section 8).
No divergence.

Group AR4A of round ten's sixth wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) section 18): the
band `0x4249D0..0x426560`, whole areas as `tools/area_rows.py --groups` cut
them. What each area *is* in the story is not read here: the names come
from what the code does.

## 1. The band, the areas, the function count

`analysis/area_funcs.tsv` (group `AR4A`) and the tool's `--unit AREA152`
.. `AREA167 --clones` list 48 starts, none ours. **All 48 are functions and
all are taken: no start dropped, none added.** Every one was read to its
last instruction with capstone (the scratch `adis.py`); every gap between
them is `nop` padding, but for tail kind 32's own byte table after its
jump table (section 6). The tool's clone rows match a scratch E8 / E9 scan
of the reading site for site, and its one jump table (`0x4260F0`, `{0x20,
0x318, 14}`) too. Areas 156..165 have no code in the band.

| Area | Descriptor | Roots | Block | Fns |
|--:|---|---|---|--:|
| 152 | `0x637300` (a choice table `0x6372FC` -> `Area130_ChoiceTailState2`, and the init) | init; `WorldMap_Records` record 10 (`0x653A28`, area byte `0x98`: `+0`, `+4`, `+8`, `+0xC`, `+0x10`, cells `+0x14` `0x637294`); `WorldMap_FieldHooks` entry 10 (`0x662E18`); six state tables in its data block | `0x4249D0..0x425B13` | 27 |
| 153 | `0x6383D8` (choice table `0x6383D4`; init `0x437CC0`, a bare `ret` in another block) | choice 0 | `0x425B20..0x425B5B` | 1 |
| 154 | `0x638950` (the same shape; init `0x437CC0`) | choice 0 | `0x425B60..0x425B9B` | 1 |
| 155 | `0x638C80` (choice table `0x638C7C`, no init) | choice 0; `0x425C30`, the choice seven areas share | `0x425BA0..0x425C57` | 2 |
| 166 | `0x63A758` (choice table `0x63A74C`: `0x425C30`, then its own) | choice 1 | `0x425C60..0x425CBE` | 1 |
| 167 | `0x63C550` (choices `0x63C510`, handlers `0x63C524`, init) | 4 choices, 8 of the 11 handlers, tail kind 32, the arrive hook, the cell hook, the init | `0x425CC0..0x42655C` | 16 |

**Shared bodies** (keyed by address, taken once, named by this band's area):

| PC | Name | Reached by |
|---|---|---|
| `0x4253C0` | `Area152_Record8Spawn` | the record-8 state table's entry 0 of all ten world maps with such a table (areas 16, 33, 45, 65, 87, 88, 115, 121, 151, 152) |
| `0x424BA0` | `Area152_PlateStart` | area 152's plate state 0 and area 151's (`0x6371A4`, AR3G's table: the linker folded the two identical bodies, as areas 104 and 121) |
| `0x425C30` | `Area155_ChoiceArmTail10` | the choices of areas 1 (`[1]`), 23 (`[1]`), 40 (`[6]`), 41 (`[2]`), 143 (`[7]`), 166 (`[0]`), 168 (`[2]`) |
| `0x425DB0` | `Area167_SetByteFE2` | area 167's handler 0 (= choice 5), area 77's handler 20 / choice 22, area 148's handler 3 / choice 5 |

**Area 167's two tables run into each other** (round10 doc section 16's
shape): the choice array `0x63C510` is five dwords before the handler array
`0x63C524`, so choices 5..15 are handlers 0..10. Handlers 1 and 8 are
AR3F's `Area146_ClearFlag46` (`0x422350`) and `Area146_ToExtraObject0`
(`0x422370`), handler 9 is `0x42C8A0` (group AR4F's band this wave); none
is called by this band's code.

**Areas 153 and 154's inits** are `0x437CC0` (the bare `ret` AR3F found as
`Area145_Init`); the sibling's `names/area_records.toml` gives both areas a
PSX init at `0x801F2C5C`. For the owner and the divergence map, as
`Area145_Init` was (round10 doc section 16).

## 2. Area 152: the world map's eleventh copy, and what differs

Area 152 is `WorldMap_Records` record 10 and `WorldMap_FieldHooks` entry
10 - the last of the eleven world maps. A capstone compare of its 25
world-map functions against area 87's ([`area_w2b.md`](area_w2b.md)
section 4; the scratch `cmp.py`: instruction by instruction, addresses
inside a function made relative) finds **24 of the 25 the same** but for
jump targets, calls to its own copies and table operands, and:

| Function | Area 87 | Area 121 | Area 152 |
|---|---|---|---|
| field hook (`PlaceMessage`) | `0x174` bytes: on a `0xA1` cell the place row's message by `(row << 4) + Cond_ByteFA`, else a cell record's name set to `Text_Records` rows | `0x74` bytes: `Msg_OpenScript(index of the place in four words + 1)` | **`0x87` bytes, a third body**: area 121's shape (no `AreaMap_ByteAt`, no cell, no name set) opening area 87's place-row message - the row of the place `0x937F82` among three rows of `0x20` (searched in bounds; 3 when none), `Msg_OpenScript(rows[(row << 4) + (s8) Cond_ByteFA])` |
| `PlateStart` | bank `0x156` | `0x158` | **`0x1D1`** (an imm32, as 87's) |
| `DrawDrift` | `0x462` bytes (a square and the map-item grid) | `0x295` bytes (two squares) | area 87's, `0x462` bytes |
| `DrawHud` | label cell `0x803580` | `0x803580` | `0x803580` |

So area 152's copy is area 87's with the bank `0x1D1` and a field hook that
always opens its place's chapter message (area 87's does so only on a
place cell). Ours is area 87's body copied (the fourth copy in ours:
`area_w2b`, `area_w2e`, `area_w3a`, `area_w3b` hold the others; sharing one
is the round's owed tidy-up) over a `WorldMapTables` for area 152
(`area_w4a_callees.h`), with a named entry per address:

| Function | Area 87 | Area 152 | Size |
|---|---|---|--:|
| `PlaceMessage` (field hook; PSX `0x801F2CA8`) | `0x40FC60` | `0x424A30` | `0x174` / `0x87` |
| `PlateRun` (record `+0`; PSX `0x801F2DB0`) | `0x40FDE0` | `0x424AC0` | `0xD6` |
| `PlateStart` / `Show` / `Grow` / `Hold` / `Shrink` | `0x40FEC0..` | `0x424BA0`, `0x424C00`, `0x424D50`, `0x424DA0`, `0x424E00` | `0x51`, `0x142`, `0x41`, `0x58`, `0x50` |
| `HudRun` (record `+0xC`; PSX `0x801F32E0`) / `HudFrame` | `0x410170` / `0x410190` | `0x424E50` / `0x424E70` | `0x12`, `0xA` |
| `FrameStep` / `SlideIn` / `Hold` / `SlideOut` | `0x4101A0..` | `0x424E80`, `0x424EA0`, `0x424ED0`, `0x424F00` | `0x12`, `0x21`, `0x28`, `0x41` |
| `BoxStep` / `SlideIn` / `Hold` / `SlideOut` | `0x410270..` | `0x424F50`, `0x424F70`, `0x424FE0`, `0x425050` | `0x12`, `0x62`, `0x6E`, `0x57` |
| `DrawFrame` / `DrawSprite` / `DrawHud` | `0x4103D0` / `0x4105A0` / `0x410660` | `0x4250B0` / `0x425280` / `0x425340` | `0x1C5`, `0xBC`, `0x58` |
| `Record8Run` (record `+8`; PSX `0x801F4038`) / `Record8Place` | `0x4106C0` / `0x4106E0` | `0x4253A0` / `0x425470` | `0x12`, `0x154` |
| `Record4Run` (record `+4`; PSX `0x801F448C`) / `Record4MarkCell` | `0x410840` / `0x410860` | `0x4255D0` / `0x4255F0` | `0x12`, `0xB5` |
| `DrawDrift` (record `+0x10`; PSX `0x801F4628`) | `0x410920` | `0x4256B0` | `0x462` |

PSX twins from the sibling's `names/area_records.toml` (area 152's init and
hooks 0..6, paired by the record slot they fill). The state tables' shared
entries are the other copies': `WorldMapHud_Start` `0x419110`,
`WorldMap_FrameWait` `0x411310`, `WorldMapHud_BoxWait` `0x414BB0`,
`Area65_Record8Move` `0x40C490`, `Area45_Record4Tick` `0x408990` - none
called by ours.

**The tables** (`[[data]]`; the plate animations and the cell records
before area 152's descriptor, the rest after it):

| Table | At | What |
|---|---|---|
| `Area152_PlateAnims` | `0x637288` | (u16 place, u8 animation, u8) x 3, searched with no bound |
| `Area152_Cells` | `0x637294` | (x, z, -, -) x 2, then the descriptor's `+0x20` data: `Record4MarkCell` by `+0xB`, unchecked |
| `Area152_PlaceRows` | `0x637344` | 3 rows of `0x20` (u16 place, fifteen u16 messages) |
| `Area152_PlateStates` / `HudStates` / `FrameStates` / `BoxStates` | `0x6373A4` / `0x6373B8` / `0x6373C0` / `0x6373D0` | 5 / 2 / 4 / 4 |
| `Area152_Sprites` / `Buttons` | `0x6373E0` / `0x637438` | 22 x 4 / 6 (read to 8) |
| `Area152_Record8States` / `Directions` / `Record8Anims` / `Record4States` | `0x637450` / `0x63745C` / `0x63746C` / `0x637474` | 3 / 4 x 4 / 4 x 2 / 2 |
| `Area152_DriftUV` | `0x63747C` | u base, v base, cell size (four bytes each) |

## 3. Area 152's own: the init and the record-8 spawn

| PC | Name | Size | Root | What |
|---|---|--:|---|---|
| `0x4249D0` | `Area152_Init` | `0x58` | init (PSX `0x801F2C38`) | story flags `0x75`, `0x76`, `0x8B`, `0x8C`, `0x8D`, `0x8E` cleared, in that order |
| `0x4253C0` | `Area152_Record8Spawn` | `0xAD` | the record-8 state 0 of all ten tables | `Field_StatusBits` bit 0: `Effect_Release`. Else only when `Frame_Counter & 0x3FF` is 0: `r = Rand() & 0x33`, `n = r & 0xF` (0..3); up to `n` `Effect_FindFree` slots (`0xFF` ends it) get `+0 = 1`, kind `+5 = 0x16`, `+1 = 1`, `+6` = the count so far, `+8 = r >> 4`, and the word `+0x3E` = `AreaMap_Elevation(leader +0x34, +0x38) + 0x400` |

So the world maps' record-8 effect (an object the map's `+8` record
places) spawns up to three kind-`0x16` effects above the leader's ground
once every 1024 frames. `n` is read as a dword off the stack whose three
upper bytes were never written; the `& 0xF` keeps only the stored byte's
low nibble, so nothing depends on them.

## 4. Areas 153..155 and 166: choices

| PC | Name | Size | Root | What |
|---|---|--:|---|---|
| `0x425B20` | `Area153_ChoiceFocusPair` | `0x3C` | area 153 choice 0 | `Area90_ChoiceFocusPair`'s shape: message `0xFFFF`; the dwords `+0x18` / `+0x1C` of the object the pointer `0x903804` names = the bytes `Area153_ChoicePairs` (`0x63841C`) `[s8 answer * 2]`, `[* 2 + 1]` (zero-extended, unchecked; pointer and answer read again for the second) |
| `0x425B60` | `Area154_ChoiceFocusPair` | `0x3C` | area 154 choice 0 | the same over `Area154_ChoicePairs` `0x638994` (byte for byte but the table) |
| `0x425BA0` | `Area155_ChoiceGiveItem4D` | `0x88` | area 155 choice 0 | answer byte 2: `Item_NamePtr(1, 0x4D)`'s 16 bytes to `Text_Records` row 0, then `Inventory_Add(1, 0x4D, 1)` (a fourth word, 0, pushed and never read) answering al not 0: `Sound_PlayEffect(0x106)`, message `0xB`, bit 1 of the bank `0x903FF8` set. Any other answer, or not added: message `0xA`, the same bit set |
| `0x425C30` | `Area155_ChoiceArmTail10` | `0x28` | shared (section 1) | message `0xFFFF`; answer byte 0: tail kind `0x9039F3 = 0xA` (`0x56DB80`, engine), state 0, argument `0xFF` |
| `0x425C60` | `Area166_ChoiceFocusPair` | `0x5F` | area 166 choice 1 | message `0xFFFF`; answer byte 3: `ScriptFlags_Set40`, tail kind `0xA` at state 5, argument `0xFF`; then the focus pair from `Area166_ChoicePairs` `0x63A79C` (the answer read again after the call) |

## 5. Area 167: choices and handlers

`Cond_ByteFD` is the byte `0x8034F1`; variables 5 and 6 are the
movement-script bytes `0x90384A` / `0x90384B`; "the script word" is
`MoveScript_Object +0xA` (the running op's position). Every pointer is read
again for each store, as the original reads it.

| PC | Name | Size | Root (PSX) | What |
|---|---|--:|---|---|
| `0x425CC0` | `Area167_ChoiceTail32At15` | `0x38` | choice 0 | message `0xFFFF`; answer byte 2 nothing more; else variable 5 = `(answer != 0) + 1`, `ScriptFlags_Set40`, variable 6 = `0x40`, tail kind `0x20` (`Area167_Tail32`) at state `0x15` |
| `0x425D00` | `Area167_ChoiceTail32At14Even` | `0x39` | choice 1 | the same with variable 5 = 2 or 0 (`neg / sbb / and 2`) and state `0x14` |
| `0x425D40` | `Area167_ChoiceTail32At14` | `0x37` | choice 2 | the same with variable 5 = `answer != 0` and state `0x14` |
| `0x425D80` | `Area167_ChoiceTail32AtA` | `0x2D` | choices 3 and 4 | message `0xFFFF`; answer byte 0: `ScriptFlags_Set40`, variable 6 = `0x60`, tail kind `0x20` at state `0xA` |
| `0x425DB0` | `Area167_SetByteFE2` | `0x8` | handler 0 (`0x801F3F14`) | `Cond_ByteFE = 2` |
| `0x425DC0` | `Area167_HeightBy4B4C` | `0xDE` | handler 2 (`0x801F3F58`) | `Cond_ByteFD` 2: flag `0x4C` set - cleared, `MoveScript_Object +3 = 5`, the script word `0xFFFE`, done; flag `0x4B` clear - `Sprite_Current` word `+0x3E = 0xFDC0`, `+0 |= 0x40`, done. `Cond_ByteFD` (read again) 1: flag `0x4C` set - cleared, `+3 = 4`, the word `0xFFFE`, done; `0x4B` set - `+0x3E = 0x9C0`, `+0 |= 0x40`, done. Else `+0x3E = 0x3C0` |
| `0x425EA0` | `Area167_Toggle4BUnlessLow` | `0x2F` | handler 3 (`0x801F4080`) | `+0x3E` at `0x3C0`: the script word + 8; else `+0` bit 6 cleared, `Flags_Toggle(0x904030, 0x4B)` |
| `0x425ED0` | `Area167_Skip3Unless4B` | `0x1E` | handler 4 (`0x801F40F4`) | flag `0x4B` clear: the script word + 3 |
| `0x425EF0` | `Area167_HeightBy48To4A` | `0x11E` | handler 5 (`0x801F4140`) | `Cond_ByteFD` 2: flag `0x4A` set - cleared, the script word `0xFFFE`, `+3` = 7 (byte `0x905E68` 0) or 8, done; flag `0x48` clear - `+0x3E` = `0x9C0` (flag `0x49` clear) or `0xFDC0`, `+0 |= 0x40`, done. Then 1: flag `0x4A` set - cleared, `+3 = 7`, the word `0xFFFE`, done; flag `0x49` clear - `+0x3E = 0x9C0`, `+0 |= 0x40`, done. Else `+0x3E = 0x3C0` |
| `0x426010` | `Area167_Flags48By49` | `0x84` | handler 6 (`0x801F42C4`) | `+0x3E` at `0x3C0`: the script word + 8; else `+0` bit 6 cleared and by `Cond_ByteFD` (read once): 1 flag `0x48` set, `0x49` cleared; 2 the other way; 3 both cleared |
| `0x4260A0` | `Area167_Skip3ByChapter` | `0x33` | handler 7 (`0x801F43B8`) | `Cond_ByteFD` 2: variable 5 not 0 done, else the script word + 3; then `Cond_ByteFD` (read again) 1: + 3 |
| `0x4260E0` | `Area167_SetByteFE10` | `0x8` | handler 10 (`0x801F4480`) | `Cond_ByteFE = 0x10` |

The handlers work as a pair of story flags (`0x48` / `0x49`) and a
chapter byte choosing among three heights (`0x9C0`, `0xFDC0`, `0x3C0`) for
the object running the script, and a script position that skips ahead
when the object is already at `0x3C0`; flags `0x4A`..`0x4C` are one-shot
requests the tail kind sets and these handlers consume. What the heights
are in the scene is not read here.

## 6. Area 167: tail kind 32, the hooks, the init

`0x4260F0` (`Area167_Tail32`, `0x350` bytes) is `Field_ModeTailKinds[32]`
(`0x662D68`): the s8 state `0x9039F4` above `0x20` (unsigned) does nothing,
else a byte table in the code (`0x426440`, 33 cases, read in place) picks
one of the fourteen entries of the jump table `0x426408`:

| State | What |
|---|---|
| 0 | `Party_DropIn(0)`, state 1 |
| 1 | variable 6 `0x18`: kind and state 0, flag `0x46` set, `Field_ChangeArea(0x95, 0x80000, 0xC0000, 0x81)` |
| 5 | `Sound_PlayEffect(0x204)`, state 6, the countdown word `0x9039F6` = `0xA` |
| 6 | the countdown - 1; at 0 `ScriptFlags_Clear40`, kind and state 0 |
| `0xA` | variable 6 `0x64`: `Party_DropIn(5)`, state `0xB` |
| `0xB` | variable 6 `0x74`: `Field_ChangeArea(0xA7, 0x130000, 0x9A0000, 0x87)` with `Cond_ByteFD` 2, else `(0xA7, 0xA0000, 0x1E0000, 0x86)`; flag `0x4B` toggled, `0x4C` set, state `0xC` |
| `0xC` | variable 6 0: `Sound_PlayEffect(0x206)` with `Cond_ByteFD` 0; variable 5 = 0, kind and state 0 |
| `0x14` | variable 6 `0x44`: `Party_DropIn(2)`, state `0x18` |
| `0x15` | `Party_DropIn(8)`, state `0x18` |
| `0x18` | variable 6 `0x54`: by variable 5 (read once) - 0: variable 6 = 0, `ScriptFlags_Clear40`, `Field_ChangeArea(0xA7, 0x1A8000, 0x560000, 5)`, flags `0x48`, `0x49` cleared; 1: `(0xA7, 0x200000, 0xF0000, 0x83)`, `0x48` set, `0x49` cleared; 2: `(0xA7, 0x290000, 0x8B0000, 0x84)`, `0x48` cleared, `0x49` set; then `Sound_PlayEffect(0x205)` with `Cond_ByteFD` 0, flag `0x4A` set, state `0xC` |
| `0x1E` | state `0x1F` |
| `0x1F` | `MoveScript_WaitWordDA` 0: `Cond_ByteFE = 0x20`, the countdown `0x1E`, state `0x20` |
| `0x20` | the countdown - 1; at 0 `ScriptFlags_Clear40`, flag `0x90` set, `Field_ChangeArea(0xA8, 0x150000, 0x40000, 7)`, `Field_ScriptFlags2`'s low byte `|= 0x40`, kind and state 0 |
| others (2..4, 7..9, `0xD..0x13`, `0x16`, `0x17`, `0x19..0x1D`) | nothing |

Ours switches on the in-code byte table read in place and aborts on a
case byte past the fourteen (the table is code: only a changed exe could
make one). `Field_ChangeArea`'s first argument (its `area` parameter in
`symbols.toml`) is `0xA7`, `0xA8` or `0x95` here: areas 167, 168 and 149 as
numbers.

| PC | Name | Size | Root | What |
|---|---|--:|---|---|
| `0x426470` | `Area167_ArriveHook` | `0x2E` | `Area_ArriveHook`'s case (`call` at `0x56E567`) | `(x, z)` 16.16: z's high word `0x66` and x's high word `0x14..0x16` (u16 compare of `x_hi - 0x14` below 3): `ScriptFlags_Set40`, tail kind `0x20` at state 0, al 1; else al 0 |
| `0x4264A0` | `Area167_CellHook` | `0x6C` | `Area_CellHooks` (pair at `0x662FD0`) | `(x, z)` as bytes: `Area167_Switch` (`0x63C594`, one `(x, z, facing nibble, flag)` record) searched in bounds for x, z and a low nibble equal to the leader's **whole** facing byte `+8` (read once before the search); none al 0; else `Flags_Toggle(0x904030, flag)`, `ScriptFlags_Set40`, tail kind `0x20` at state 5, al 1 |
| `0x426510` | `Area167_Init` | `0x4D` | init (PSX `0x801F49F0`) | `Cond_ByteFD` 0, flag `0x8F` set and `0x90` clear: `Camera_Distance = 0x980`, tail kind `0x20` at state `0x1E` |

So area 167's floor switch (the cell hook) toggles its flag and plays
states 5 and 6 (a sound, then ten frames and the tail ends); its choices
and arrive hook start the drop-in and area-change states; its entry, in
the first chapter value with flag `0x8F` set and `0x90` not, pulls the
camera in and runs states `0x1E..0x20` (wait for the movement script, 30
frames, then flag `0x90` and area 168).

## 7. Latent defects (described, not fixed)

The owner's rule (round9 doc section 6): ours aborts where the original
would fault; a silent read past a table into mapped memory is reproduced
as the original makes it.

1. **`Area152_PlaceMessage` reads its row unchecked.** A place in none of
   the three rows gives row 3, which is `Area152_PlateStates`: the message
   id is then a half of a code pointer (which half, by the chapter byte); a
   chapter byte of 16 or more reads the next row's words,
   a negative one the bytes before the rows. Reproduced (all in `.data`).
2. **The world map's**: the unbounded plate search (`Area152_PlateAnims`
   has three entries), `Area152_Cells` by `+0xB` unchecked (two records,
   then the descriptor's data), the unchecked `.data` dispatches (ours
   aborts past each of the six tables) - area 45's set,
   [`area_w1b.md`](area_w1b.md).
3. **`Area152_Record8Spawn` writes an `Effect_FindFree` slot unchecked**
   against the twenty records (it stops only on `0xFF`); ours aborts past
   twenty. It reads its count as a stack dword with three unwritten bytes
   (harmless: masked to the stored byte's nibble).
4. **The focus-pair choices index their pair tables by the s8 answer
   unchecked** (153, 154, 166; `Area53_` / `Area90_` / `Area133_` have the
   same): a negative answer reads before the table. Reproduced.
5. **`Area167_CellHook` compares a nibble with the leader's whole facing
   byte**: a facing byte with any of bits 4..7 set never matches.
   Reproduced (the fuzz plants such facings).

## 8. What reaches it, calls across groups

- **Reach - no recorded route reaches any of the 48.** A scan of every
  `analysis/calltrace/*/*.tsv` for addresses in `0x4249D0..0x426560` finds
  none (the world-map route plays area 33's copy). Fuzz only.
- **Cross-group raw-address calls: none.** Every callee is named and ours
  (`Flags_*` including SX2's `Flags_Toggle`, `ScriptFlags_Set40` /
  `_Clear40`, `Party_DropIn`, `Field_ChangeArea`, `Item_NamePtr`,
  `Inventory_Add`, `Sound_PlayEffect`, `Msg_OpenScript`, `Effect_FindFree`,
  `Effect_Release`, `AreaMap_Elevation`, `AreaMap_ByteAt`,
  `WorldMap_PinSprite`, `WorldMap_DrawNeedle`, `WorldMap_RecordIndex`,
  `Field_CellHasEvent`, `MapView_ItemHalfAt`, `MapView_LinkPrimAt`,
  `Prim_SetTexture`, `Text_DrawAt`, the `Gpu_*` / `Gte_*` / `Sprite_*` set)
  or Capcom's in the harness's standard set (`Rand`). No harness edit.
- **Inbound from outside the band** (for the rebinding pass; an E8 / E9
  and dword scan of the image): engine `0x56E567` (a `call`,
  `Area_ArriveHook`) into `Area167_ArriveHook`; `Field_ModeTailKinds[32]`
  (`0x662D68`) names `Area167_Tail32`; `Area_CellHooks`' pair at
  `0x662FD0` names `Area167_CellHook`; `WorldMap_Records[10]` and
  `WorldMap_FieldHooks[10]` name area 152's six roots; the record-8 tables
  of areas 16, 33, 45, 65, 87, 88, 115, 121 and 151 (`0x5E6008`,
  `0x5EF688`, `0x5F7700`, `0x60492C`, `0x6126C8`, `0x613044`, `0x6203BC`,
  `0x6246D8`, `0x637250`) name `Area152_Record8Spawn`; area 151's plate
  table (`0x6371A4`, AR3G's) names `Area152_PlateStart`; the choice tables
  of areas 1, 23, 40, 41, 143, 168 (`0x5DC750`, `0x5E8734`, `0x5F4BC8`,
  `0x5F5EDC`, `0x630C64`, `0x63C8E4`) name `Area155_ChoiceArmTail10`;
  areas 77 and 148's descriptors (`0x60BB84`, `0x635378`) name
  `Area167_SetByteFE2`. All are read in place: no rebinding owed but the
  engine `call`.
- `analysis/calltrace/entries_logic.txt`: 46 lines appended; `004250B0
  1C5` and `00425280 BC` were there. Host lines with larger extents cover
  `0x4249D0..` (`004242A0 BDA`), `0x424E80` (`227`), `0x425340` (`1121`)
  and `0x426470` (`399`): the smaller extents are appended beside them.

## 9. The fuzz

`BOF3X_SHADOW=area_w4a` (`src/game/area_w4a_fuzz.cpp`): six `Run` calls
under the one shadow name, `Group::area` each area's number, the real
descriptors and tables in place. `BOF3X_AR4A_AREA=n` runs one area's group
alone (the controls script's shortcut).

- **Area 152** (4,000 rounds a function, 27 functions): area 87's group
  ([`area_w2b.md`](area_w2b.md) section 5) over area 152's tables - the six
  state tables as `DataTable`s, a packet buffer, items and names of the
  fuzz's, the plate animation, cell, place-row, direction and drift tables
  as regions from the fuzz's own literal addresses (put back to the exe's
  bytes two rounds in three), `Effect_Objects` as a region. The field hook:
  its state 0, 1, 2 and the sign's edges; the chapter byte 0..15 two rounds
  in three, else 15, 16, -1, -16, `0x80`, `0x7F`; the place planted in a
  row, or `0xFFFF`, the rows' message words made distinct half the time
  (the shipped rows repeat one). The spawn: the status bit clear two
  rounds in three, the frame a multiple of 1024 two in three (else one of
  its low ten bits set); `Effect_FindFree`'s stand-in answering a slot of
  the twenty four calls in five, else `0xFF`, garbage above. `settle`
  keeps `+1` inside the plate table and the place in the last plate
  animation (the search has no bound).
- **Areas 153, 154, 155, 166** (6,000): the answer byte 0..3 two rounds in
  three, else 4, `0x80`, `0xFF`, `0x7F`, `0xFE`; the focus pointer a party
  record or a field object (a region of the group's), moved by `disturb`
  with the answer byte; `Text_Records` row 0 and the name buffer
  `Item_NamePtr`'s stand-in points into as regions; the three pair tables
  as regions.
- **Area 167** (8,000, 16 functions): `Cond_ByteFD` 0..3 two rounds in
  three, else 4, `0x80`, `0xFF`, `0x81`, `0x82`; `MoveScript_Object` a
  region seeded to a record; the height word `+0x3E` at `0x3C0` half the
  time (else one off, `0x13C0`, `0x9C0`, `0xFDC0`); `0x905E68` 0 or not;
  variable 5 0..2 and 3, `0x80`, `0xFF`. The tail: a state with a body two
  rounds in three with its wait value in variable 6 two times in three (or
  one off), else any state and the range's edges and sign; the countdown
  1, 2, 0, `0xFFFF`; `MoveScript_WaitWordDA` 0 or not. The arrive hook's
  positions with high words at and around `0x14..0x16` / `0x66`; the cell
  hook's the switch's bytes (with bits above, one off, or anything) and the
  leader's facing the switch's nibble, or that nibble with a bit above, or
  another. `disturb` moves the chapter byte, `MoveScript_Object`, the
  height word, variables 5 and 6 and the countdown.

**Result (in this worktree):** 0 mismatches in every run; rounds / calls to
the stand-ins: area 152 108,000 / 567,842 (every state-table entry reached:
the plate states 766..832, frame and box states about 1,000 and the frame
hold 5,019, record-8 about 1,300, record-4 about 2,000; `Msg_OpenScript`
1,015, `Rand` 2,198, `AreaMap_Elevation` 2,047); 153 and 154 6,000 each;
155 12,000 / 9,180; 166 6,000 / 1,112; 167 128,000 / 60,300
(`Field_ChangeArea` 647, `Party_DropIn` 1,299, `Flags_Toggle` 5,525).
`BOF3X_SHADOW='*'`: exit 0 on the first run, 492 self-test lines, no mismatch
or Fatal, `inject: 5406 ours` (all 48 of this group's injected; 5,358
before).

## 10. Controls

Planted one at a time in `area_w4a.cpp` (and, for the table constants,
`area_w4a_callees.h`) by a script (the scratch `controls.py` and
`controls_list.py`, not committed): each anchored on a string the file
holds once; plant, rebuild (checking `area_w4a.cpp` recompiled), run the
area's group alone (`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=area_w4a
BOF3X_AR4A_AREA=n`), restore; after the last, a rebuild and a clean full
run. The world-map set is AR2B's (its `C..` plants, renumbered `A..`)
where the body is area 87's, plus area 152's own. **224 planted, 224 refused**: 221 by a count (exit 3), 3 by ours' own
abort - C47 (the tail's state bound made signed), C89 (the next case byte)
and C112 (the case table from one byte on) make a state read a case byte
past the fourteen-entry jump table, where the original would jump through
whatever follows it and ours stops with a Fatal; their near variants C46
(the bound `0x1F`), C89b (the case byte of the even state) and C112b (the
table from one byte before, every byte still a case) are refused by a
count. None stood.

A first run of the set stood on one: B17 (area 155's `Inventory_Add` answer
tested `& 0x7F`) - the harness's standard stand-in answers a "not 0" byte
with bit 4 always set, so the byte `0x80`, the only one that tells the two
apart, never came. The group now lists `Inventory_Add` with an effect
answering 0, 1, `0x80`, `0x10` or any byte; B17 is refused by a count. The
table is the second run (in this worktree; rounds of a function's 4,000 in
area 152, 6,000 in the choices, 8,000 in area 167).

| # | area | planted | refused in rounds |
|---|--:|---|---|
| A2 | 152 | Init: 0x74 first | Area152_Init 4000 |
| A3 | 152 | PlaceMessage: state 2 handled as 1 | Area152_PlaceMessage 248 |
| A4 | 152 | PlaceMessage: waits on 3 | Area152_PlaceMessage 262 |
| A5 | 152 | PlaceMessage: 0x9039F5 kept | Area152_PlaceMessage 449 |
| A6 | 152 | PlaceMessage: chapter zero-extended | Area152_PlaceMessage 89 |
| A7 | 152 | PlaceMessage: rows of 8 words | Area152_PlaceMessage 609 |
| A8 | 152 | PlaceMessage: two rows searched | Area152_PlaceMessage 237 |
| A9 | 152 | PlaceMessage: request 3 | Area152_PlaceMessage 1015 |
| A10 | 152 | PlaceMessage: state + 2 | Area152_PlaceMessage 1015 |
| A11 | 152 | PlaceMessage: the next word | Area152_PlaceMessage 858 |
| A12 | 152 | PlaceMessage: no Set40 | Area152_PlaceMessage 1015 |
| A13 | 152 | PlaceMessage: state not read again after the call | Area152_PlaceMessage 5 |
| A14 | 152 | PlateRun: 0xA0 kind 3 | Area152_PlateRun 305 |
| A15 | 152 | PlateRun: bit 11 | Area152_PlateRun 1299 |
| A16 | 152 | PlateRun: the next state | Area152_PlateRun 4000 |
| A17 | 152 | PlateRun: z stepped by x | Area152_PlateRun 3476 |
| A18 | 152 | PlateStart: bank + 1 | Area152_PlateStart 4000 |
| A19 | 152 | PlateStart: +0x29 4 | Area152_PlateStart 4000 |
| A20 | 152 | PlateShow: kind 2 animation 2 | Area152_PlateShow 353 |
| A21 | 152 | PlateShow: +9 7 | Area152_PlateShow 2021 |
| A22 | 152 | PlateShow: +0x44 | Area152_PlateShow 2033 |
| A23 | 152 | PlateShow: the animation from the pad byte | Area152_PlateShow 970 |
| A24 | 152 | PlateGrow: step 0x1000 | Area152_PlateGrow 4000 |
| A25 | 152 | PlateGrow: no pin | Area152_PlateGrow 4000 |
| A26 | 152 | PlateHold: request 4 | Area152_PlateHold 341 |
| A27 | 152 | PlateHold: Game_Mode 2 holds | Area152_PlateHold 814 |
| A28 | 152 | PlateShrink: released on 4 | Area152_PlateShrink 247 |
| A29 | 152 | PlateShrink: back to 2 | Area152_PlateShrink 619 |
| A30 | 152 | HudRun: the other entry | Area152_HudRun 4000 |
| A31 | 152 | HudFrame: the box first | Area152_HudFrame 4000 |
| A32 | 152 | FrameStep: the next entry | Area152_FrameStep 4000 |
| A33 | 152 | FrameSlideIn: above 0x10 | Area152_FrameSlideIn 381 |
| A34 | 152 | FrameHold: mode 3 | Area152_FrameHold 648 |
| A35 | 152 | FrameSlideOut: below -0x30 | Area152_FrameSlideOut 61 |
| A36 | 152 | BoxStep: the next entry | Area152_BoxStep 4000 |
| A37 | 152 | BoxSlideIn: below 0xC8 | Area152_BoxSlideIn 135 |
| A38 | 152 | BoxHold: 0x59 frames | Area152_BoxHold 281 |
| A39 | 152 | BoxSlideOut: above 0xF0 | Area152_BoxSlideOut 372 |
| A40 | 152 | BoxLeaves: bit 9 | Area152_BoxSlideIn 968; Area152_BoxHold 890 |
| A41 | 152 | DrawFrame: second key over seven | Area152_DrawFrame 1482 |
| A42 | 152 | DrawFrame: legend 3 y + 0x17 | Area152_DrawFrame 2572 |
| A43 | 152 | DrawFrame: party set & 0xFF | Area152_DrawFrame 59 |
| A44 | 152 | DrawSprite: CLUT 0x7B81 | Area152_DrawSprite 4000 |
| A45 | 152 | DrawSprite: semi by & 0x7F | Area152_DrawSprite 5 |
| A46 | 152 | DrawHud: cap at x + 0x7F | Area152_DrawHud 2914 |
| A47 | 152 | DrawHud: label & 0xFFF | Area152_DrawHud 2748 |
| A48 | 152 | Record8Place: bank 0x47 | Area152_Record8Place 4000 |
| A49 | 152 | Record8Place: first nudge >> 12 | Area152_Record8Place 1747 |
| A50 | 152 | Record8Place: up for +6 2 | Area152_Record8Place 1529 |
| A51 | 152 | Record8Place: animation from byte 1 | Area152_Record8Place 2995 |
| A52 | 152 | Record4MarkCell: cell 0xA1 | Area152_Record4MarkCell 2822 |
| A53 | 152 | Record4MarkCell: released on 8 | Area152_Record4MarkCell 1110 |
| A54 | 152 | Record4MarkCell: bank 0x204 | Area152_Record4MarkCell 2822 |
| A55 | 152 | Record4MarkCell: z from byte 0 | Area152_Record4MarkCell 2539 |
| A56 | 152 | DrawDrift: wrap to -7 | Area152_DrawDrift 1604 |
| A57 | 152 | DrawDrift: x within 24 | Area152_DrawDrift 519 |
| A58 | 152 | DrawDrift: CLUT 0x78CC | Area152_DrawDrift 918 |
| A59 | 152 | DrawDrift: commit 0x44 | Area152_DrawDrift 1714 |
| A60 | 152 | DrawDrift: size from the u table | Area152_DrawDrift 913 |
| A61 | 152 | DrawDrift: +0x38 by b << 9 | Area152_DrawDrift 2989 |
| A62 | 152 | DrawDrift: texture bit 0 | Area152_DrawDrift 857 |
| A63 | 152 | Record8Run: the next entry | Area152_Record8Run 4000 |
| A64 | 152 | Record4Run: the other entry | Area152_Record4Run 4000 |
| A65 | 152 | Spawn: every 512 frames | Area152_Record8Spawn 306 |
| A66 | 152 | Spawn: status bit 1 | Area152_Record8Spawn 2003 |
| A67 | 152 | Spawn: Rand & 0x37 | Area152_Record8Spawn 858 |
| A68 | 152 | Spawn: n & 1 | Area152_Record8Spawn 789 |
| A69 | 152 | Spawn: kind 0x17 | Area152_Record8Spawn 1183 |
| A70 | 152 | Spawn: +6 from 1 | Area152_Record8Spawn 1183 |
| A71 | 152 | Spawn: +8 r >> 3 | Area152_Record8Spawn 872 |
| A72 | 152 | Spawn: ground + 0x3FF | Area152_Record8Spawn 1183 |
| A73 | 152 | Spawn: (z, x) asked | Area152_Record8Spawn 1183 |
| A74 | 152 | Spawn: none goes on | Area152_Record8Spawn 260 |
| A75 | 152 | Spawn: +1 2 | Area152_Record8Spawn 1183 |
| A76 | 152 | kWm152: bank 0x1D2 | Area152_PlateStart 4000 |
| A77 | 152 | kWm152: two place rows | Area152_PlaceMessage 237 |
| A78 | 152 | kWm152: plate animations from the second | Area152_PlateShow 386 |
| A79 | 152 | kWm152: cells from the second | Area152_Record4MarkCell 2691 |
| A80 | 152 | kWm152: sprites from the second | Area152_DrawSprite 4000 |
| A81 | 152 | kWm152: directions from the second | Area152_Record8Place 4000 |
| A82 | 152 | kWm152: drift u from the next | Area152_DrawDrift 759 |
| A83 | 152 | kWm152: buttons from the second | Area152_DrawFrame 720 |
| A84 | 152 | kWm152: label cell 0x803584 | Area152_DrawHud 2914 |
| A85 | 152 | kWm152: record-8 animations from the second | Area152_Record8Place 3993 |
| B1 | 153 | FocusPair: second from the next pair | Area153_ChoiceFocusPair 5979 |
| B2 | 153 | FocusPair: answer unsigned | Area153_ChoiceFocusPair 836 |
| B4 | 153 | FocusPair: message 0xFFFE | Area153_ChoiceFocusPair 6000 |
| B5 | 153 | FocusPair: +0x20 | Area153_ChoiceFocusPair 6000 |
| B6 | 154 | 154: area 153's pairs | Area154_ChoiceFocusPair 4789 |
| B7 | 153 | kA153Pairs from the second pair | Area153_ChoiceFocusPair 6000 |
| B8 | 155 | GiveItem: answer 3 | Area155_ChoiceGiveItem4D 2253 |
| B9 | 155 | GiveItem: name 0x4E | Area155_ChoiceGiveItem4D 1137 |
| B10 | 155 | GiveItem: count 2 | Area155_ChoiceGiveItem4D 1137 |
| B11 | 155 | GiveItem: sound 0x107 | Area155_ChoiceGiveItem4D 732 |
| B12 | 155 | GiveItem: message 0xC | Area155_ChoiceGiveItem4D 700 |
| B13 | 155 | GiveItem: message 9 | Area155_ChoiceGiveItem4D 5054 |
| B14 | 155 | GiveItem: bit 2 on the add | Area155_ChoiceGiveItem4D 732 |
| B15 | 155 | GiveItem: 12 name bytes | Area155_ChoiceGiveItem4D 1137 |
| B16 | 155 | GiveItem: bank 0x903FF0 | Area155_ChoiceGiveItem4D 6000 |
| B17 | 155 | GiveItem: answer & 0x7F | Area155_ChoiceGiveItem4D 200 |
| B18 | 155 | ArmTail10: answer 1 arms | Area155_ChoiceArmTail10 1769 |
| B19 | 155 | ArmTail10: state 1 | Area155_ChoiceArmTail10 1173 |
| B20 | 155 | ArmTail10: argument 0xFE | Area155_ChoiceArmTail10 1173 |
| B21 | 155 | ArmTail10: kind 0xB | Area155_ChoiceArmTail10 1173 |
| B22 | 166 | 166: answer 2 arms | Area166_ChoiceFocusPair 2246 |
| B23 | 166 | 166: state 4 | Area166_ChoiceFocusPair 1090 |
| B24 | 166 | 166: no Set40 | Area166_ChoiceFocusPair 1090 |
| B25 | 166 | 166: answer read before the call | Area166_ChoiceFocusPair 10 |
| B26 | 166 | 166: first from byte 1 | Area166_ChoiceFocusPair 5149 |
| B27 | 166 | 166: argument 0x7F | Area166_ChoiceFocusPair 1090 |
| C1 | 167 | At15: variable 5 + 2 | Area167_ChoiceTail32At15 6175 |
| C2 | 167 | At15: answer 3 ends | Area167_ChoiceTail32At15 2658 |
| C3 | 167 | At15: state 0x16 | Area167_ChoiceTail32At15 6222 |
| C4 | 167 | At15: variable 6 0x41 | Area167_ChoiceTail32At15 6222 |
| C5 | 167 | At14Even: 1 | Area167_ChoiceTail32At14Even 4358 |
| C6 | 167 | At14Even: state 0x15 | Area167_ChoiceTail32At14Even 6185 |
| C7 | 167 | At14: answer 1 only | Area167_ChoiceTail32At14 3532 |
| C8 | 167 | At14: no Set40 | Area167_ChoiceTail32At14 6207 |
| C9 | 167 | AtA: variable 6 0x61 | Area167_ChoiceTail32AtA 1791 |
| C10 | 167 | AtA: answer 1 | Area167_ChoiceTail32AtA 2700 |
| C11 | 167 | AtA: state 0xB | Area167_ChoiceTail32AtA 1791 |
| C12 | 167 | AtA: message 0xFFFE | Area167_ChoiceTail32AtA 7920 |
| C13 | 167 | SetByteFE2: 3 | Area167_SetByteFE2 8000 |
| C14 | 167 | SetByteFE10: 0x11 | Area167_SetByteFE10 8000 |
| C15 | 167 | HeightBy4B4C: chapter 3 | Area167_HeightBy4B4C 2661 |
| C16 | 167 | HeightBy4B4C: +3 6 | Area167_HeightBy4B4C 1141 |
| C17 | 167 | HeightBy4B4C: 0xFDC1 | Area167_HeightBy4B4C 211 |
| C18 | 167 | HeightBy4B4C: bit 5 | Area167_HeightBy4B4C 128 |
| C19 | 167 | HeightBy4B4C: chapter 1 +3 3 | Area167_HeightBy4B4C 1186 |
| C20 | 167 | HeightBy4B4C: 0x9C1 | Area167_HeightBy4B4C 398 |
| C21 | 167 | HeightBy4B4C: end 0x3C1 | Area167_HeightBy4B4C 5064 |
| C22 | 167 | HeightBy4B4C: script word 0xFFFD | Area167_HeightBy4B4C 1141 |
| C23 | 167 | HeightBy4B4C: flag 0x4D tested | Area167_HeightBy4B4C 1736 |
| C24 | 167 | HeightBy4B4C: chapter 1 not cleared | Area167_HeightBy4B4C 1186 |
| C25 | 167 | Toggle4B: 0x3C1 | Area167_Toggle4BUnlessLow 3236 |
| C26 | 167 | Toggle4B: skip 7 | Area167_Toggle4BUnlessLow 2677 |
| C27 | 167 | Toggle4B: bit 7 | Area167_Toggle4BUnlessLow 4627 |
| C28 | 167 | Toggle4B: flag 0x4C | Area167_Toggle4BUnlessLow 5323 |
| C29 | 167 | Skip3Unless4B: 4 | Area167_Skip3Unless4B 2701 |
| C30 | 167 | Skip3Unless4B: inverted | Area167_Skip3Unless4B 8000 |
| C31 | 167 | HeightBy48: chapter 3 | Area167_HeightBy48To4A 2662 |
| C32 | 167 | HeightBy48: 0x905E68 inverted | Area167_HeightBy48To4A 1156 |
| C33 | 167 | HeightBy48: 9 | Area167_HeightBy48To4A 555 |
| C34 | 167 | HeightBy48: heights swapped | Area167_HeightBy48To4A 206 |
| C35 | 167 | HeightBy48: 0x48 inverted | Area167_HeightBy48To4A 602 |
| C36 | 167 | HeightBy48: chapter 1 +3 6 | Area167_HeightBy48To4A 1215 |
| C37 | 167 | HeightBy48: chapter 1 0x49 inverted | Area167_HeightBy48To4A 607 |
| C38 | 167 | HeightBy48: clears 0x4B | Area167_HeightBy48To4A 1156 |
| C39 | 167 | Flags48By49: skip 9 | Area167_Flags48By49 2677 |
| C40 | 167 | Flags48By49: chapter 1 the other way | Area167_Flags48By49 1126 |
| C41 | 167 | Flags48By49: chapter 4 | Area167_Flags48By49 974 |
| C42 | 167 | Flags48By49: bit 6 kept | Area167_Flags48By49 4034 |
| C43 | 167 | Skip3ByChapter: variable 5 ignored | Area167_Skip3ByChapter 1205 |
| C44 | 167 | Skip3ByChapter: 2 | Area167_Skip3ByChapter 1756 |
| C45 | 167 | Skip3ByChapter: chapter 3 | Area167_Skip3ByChapter 875 |
| C46 | 167 | Tail32: above 0x1F | Area167_Tail32 656 |
| C47 | 167 | Tail32: a signed bound | exit 3: FATAL: Area167_Tail32: case byte 52 (state -32) is past the fourteen-entry jump table 0x426408 |
| C48 | 167 | Tail32: drop-in 1 | Area167_Tail32 464 |
| C49 | 167 | Tail32: state 2 after 0 | Area167_Tail32 464 |
| C50 | 167 | Tail32: wait 0x19 | Area167_Tail32 239 |
| C51 | 167 | Tail32: flag 0x47 | Area167_Tail32 195 |
| C52 | 167 | Tail32: area 0x96 | Area167_Tail32 195 |
| C53 | 167 | Tail32: flags 0x80 | Area167_Tail32 195 |
| C54 | 167 | Tail32: sound 0x203 | Area167_Tail32 447 |
| C55 | 167 | Tail32: countdown 0xB | Area167_Tail32 447 |
| C56 | 167 | Tail32: no Clear40 after the countdown | Area167_Tail32 92 |
| C57 | 167 | Tail32: countdown by 2 | Area167_Tail32 438 |
| C58 | 167 | Tail32: wait 0x63 | Area167_Tail32 216 |
| C59 | 167 | Tail32: drop-in 6 | Area167_Tail32 176 |
| C60 | 167 | Tail32: state 0xA again | Area167_Tail32 176 |
| C61 | 167 | Tail32: chapter 1 goes to the first | Area167_Tail32 104 |
| C62 | 167 | Tail32: z 0x9B0000 | Area167_Tail32 45 |
| C63 | 167 | Tail32: flags 0x85 | Area167_Tail32 147 |
| C64 | 167 | Tail32: flag 0x4D | Area167_Tail32 192 |
| C65 | 167 | Tail32: sound on chapter 1 | Area167_Tail32 57 |
| C66 | 167 | Tail32: variable 5 kept | Area167_Tail32 159 |
| C67 | 167 | Tail32: wait 1 | Area167_Tail32 252 |
| C68 | 167 | Tail32: drop-in 3 | Area167_Tail32 191 |
| C69 | 167 | Tail32: wait 0x45 | Area167_Tail32 250 |
| C70 | 167 | Tail32: drop-in 9 | Area167_Tail32 468 |
| C71 | 167 | Tail32: state 0x19 | Area167_Tail32 468 |
| C72 | 167 | Tail32: wait 0x55 | Area167_Tail32 216 |
| C73 | 167 | Tail32: way 0 variable 6 kept | Area167_Tail32 40 |
| C74 | 167 | Tail32: way 0 flags 4 | Area167_Tail32 42 |
| C75 | 167 | Tail32: way 1 flags 0x82 | Area167_Tail32 35 |
| C76 | 167 | Tail32: way 2 area 0xA6 | Area167_Tail32 45 |
| C77 | 167 | Tail32: way 2 sets 0x48 | Area167_Tail32 45 |
| C78 | 167 | Tail32: sound 0x204 after the ways | Area167_Tail32 14 |
| C79 | 167 | Tail32: flag 0x4B after the ways | Area167_Tail32 169 |
| C80 | 167 | Tail32: way 3 | Area167_Tail32 48 |
| C81 | 167 | Tail32: 0x1E to 0x20 | Area167_Tail32 472 |
| C82 | 167 | Tail32: wait inverted | Area167_Tail32 413 |
| C83 | 167 | Tail32: Cond_ByteFE 0x21 | Area167_Tail32 220 |
| C84 | 167 | Tail32: countdown 0x1F | Area167_Tail32 220 |
| C85 | 167 | Tail32: flag 0x91 | Area167_Tail32 138 |
| C86 | 167 | Tail32: area 0xA9 | Area167_Tail32 138 |
| C87 | 167 | Tail32: bit 7 | Area167_Tail32 100 |
| C88 | 167 | Tail32: x 0x150001 | Area167_Tail32 138 |
| C89 | 167 | Tail32: the next case byte | exit 3: FATAL: Area167_Tail32: case byte 144 (state 32) is past the fourteen-entry jump table 0x426408 |
| C90 | 167 | Arrive: z 0x67 | Area167_ArriveHook 1356 |
| C91 | 167 | Arrive: four cells | Area167_ArriveHook 253 |
| C92 | 167 | Arrive: from 0x15 | Area167_ArriveHook 765 |
| C93 | 167 | Arrive: al 2 | Area167_ArriveHook 1008 |
| C94 | 167 | Arrive: state 1 | Area167_ArriveHook 1008 |
| C95 | 167 | Arrive: z's low word | Area167_ArriveHook 1008 |
| C96 | 167 | Arrive: x compared signed | Area167_ArriveHook 925 |
| C97 | 167 | Cell: facing & 7 | Area167_CellHook 5 |
| C98 | 167 | Cell: the leader's facing masked | Area167_CellHook 163 |
| C99 | 167 | Cell: (z, x) | Area167_CellHook 10 |
| C100 | 167 | Cell: flag from byte 2 | Area167_CellHook 10 |
| C101 | 167 | Cell: state 6 | Area167_CellHook 10 |
| C102 | 167 | Cell: al 2 | Area167_CellHook 10 |
| C103 | 167 | Cell: none answers 1 | Area167_CellHook 7990 |
| C104 | 167 | Cell: x a word | Area167_CellHook 3 |
| C105 | 167 | Init: chapter 1 | Area167_Init 2647 |
| C106 | 167 | Init: flag 0x8E | Area167_Init 902 |
| C107 | 167 | Init: 0x90 inverted | Area167_Init 602 |
| C108 | 167 | Init: distance 0x981 | Area167_Init 205 |
| C109 | 167 | Init: state 0x1D | Area167_Init 205 |
| C110 | 167 | Init: kind 0x21 | Area167_Init 205 |
| C111 | 167 | Cell: the switch table from 4 before | Area167_CellHook 8000 |
| C112 | 167 | Tail32: the case table from 1 on | exit 3: FATAL: Area167_Tail32: case byte 144 (state 32) is past the fourteen-entry jump table 0x426408 |
| C89b | 167 | Tail32: the case byte of the even state | Area167_Tail32 1843 |
| C112b | 167 | Tail32: the case table from 1 before | Area167_Tail32 4187 |
