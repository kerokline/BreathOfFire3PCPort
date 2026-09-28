# World 4, areas 152..155, 166..167: the world map's last copy, five choices, a flag-and-height area with its own tail kind

**Status:** IN PROGRESS (2026-09-28) - 48 functions ours
(`src/game/area_w4a.cpp`, shadow name `area_w4a`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)), one `Run` per area:
0 mismatches in 272,000 rounds (section 9, in this worktree). Controls:
section 10. Fuzz only: no recorded route reaches any of the 48 (section 8).
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

## 10. Controls

Planted one at a time in `area_w4a.cpp` (and, for the table constants,
`area_w4a_callees.h`) by a script (the scratch `controls.py` and
`controls_list.py`, not committed): each anchored on a string the file
holds once; plant, rebuild (checking `area_w4a.cpp` recompiled), run the
area's group alone (`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=area_w4a
BOF3X_AR4A_AREA=n`), restore; after the last, a rebuild and a clean full
run. The world-map set is AR2B's (its `C..` plants, renumbered `A..`)
where the body is area 87's, plus area 152's own.
