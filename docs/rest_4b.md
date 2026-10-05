# Group R4B: the community band's tail kinds and its board

**Status:** MEASURED (2026-10-05) - round fourteen
([`takeover-queue-round14.md`](takeover-queue-round14.md)), wave four, from the
round branch's tip `35ec19e`. **60 functions ours** (`src/game/rest_4b.cpp`,
declarations in `src/game/rest_4b.h`, the tables' and cells' addresses in
`src/game/rest_4b_callees.h`, shadow name `rest_4b`): the cut's 60 rows for R4B
(`analysis/round14_cut.tsv`), no start dropped and none added; each read to its
last instruction with capstone and fuzzed through the scenario harness in field
mode ([`scenario_harness.md`](scenario_harness.md) section 7), used unchanged:
360,000 rounds, 0 mismatches. **CONTROLS_SUMMARY** (section 6). **Fuzz only**:
no recorded route enters the faerie village, and the catalog shows no row of
this wave reached (section 9); the owner records a route once the code is ours.

The band is two things. **Field_ModeTailKinds 14, 21..26 and 60** (the field's
tail kinds, `Field_ModeTailKinds` `0x662CE8` by the s8 `0x9039F3`): seven
dispatchers on the s8 tail state `0x9039F4` and the states they reach - loads
(`LoadDatFile` 0x12B..0x130, a sound bank, a stream), three gifts by the
community record the tail's argument `0x9039F5` names, the waits and the ends
that put the kept object's facing back. And **the board**
(`CommuBoard_States` by `0x939A3E`, reached as `CommuTail14_States[3]`): a
grid cursor over a slot-0 row, eight slots (two rows of four, each locked past
the area's limit `0x653601[Game_AreaNumber]`) and three slots 9..11 down the
right; the 60 community records at `0x9046D0` (8 bytes: in use, the slot, two
marks, a time) placed in them; a slot's kind and two further choices picked
from lists; a yes / no row that commits them; and the panel, cards, bars and
sprites it draws. What any of this is in play is not stated here; the names say
what the code does. "Commu" follows the cut's COMMU class (the PSX COMMU
overlays, the faerie village, areas 175..185 - the brief's words, and
`places.toml`'s), not a reading of play.

PSX twins (`analysis/pairs_propagated.json`, by callers, hypotheses):
`0x457120` 0x801F1BB8 (overlay section fa8223c2), `0x458D70` 0x801D3038 and
`0x459720` 0x801D3F08 (COMMU01, section c72ce23e). The sibling's
`names/*.toml` and `symbols.toml` name none of them, so every name here is from
the PC code; the twins' addresses are cited in the evidence strings.

## 1. What each function does

The cells: the tail's kind / state / argument `0x9039F3` / `0x9039F4` (s8) /
`0x9039F5`; the field object R4A's arming handlers keep in `0x939A38`; the
board's bytes `0x939A3C` (the stream flag), `0x939A3D` (the kept music track),
`0x939A3E` (the board state), `0x939A40` (its step); the board's cursor and
picks in `.data` at `0x675F74..0x675F86` (`0x675F74` the grid cursor's mode 0 /
1 / 2 with `0x675F75` row and `0x675F76` column; `0x675F7A` the help line's
message word, 0xFFFF none; `0x675F7C` the yes / no row; `0x675F7D`,
`0x675F7E`, `0x675F7F` the picks; `0x675F80` the list cursor; `0x675F82`,
`0x675F83` the open lists, 0xFF none; `0x675F84..86` the grid cursor kept;
`0x675F78` / `0x675F81` the help and step R4C's `0x459EE0` keeps). The records
`0x9046D0 + 8k` (k < 60): `+0` in use, `+1` the slot, `+2` / `+3` marks (`+3`
two nibbles), `+4` a time from the clock `0x904134`. The slots `0x9048A8 + 8s`
(s 1..8 at `0x9048B0`): `+0` the kind (0 empty, else 4 + a pick), `+1`, `+2`,
`+3`, `+4` a time. "Help n" is `0x675F7A = n`.

### 1.1 The tail kinds

| Address | Name | What it does |
|---|---|---|
| `0x456D50` | `CommuTail14_Dispatch` | `Field_ModeTailKinds[14]`: `jmp [CommuTail14_States + 4 * s8 0x9039F4]` (11) |
| `0x456D60` | `CommuTail14_OpenF8` | `[0]`: nothing while Field_Request is 2; next state once the message box shows 0xF9; else message 0xF8, Field_Request 2 |
| `0x456D90` | `CommuTail14_LoadDat` | `[1]`: `LoadDatFile(0x12B)`, next |
| `0x456DB0` | `CommuTail_WaitLoad` | 14`[2]`, 23`[1] [10] [17]`, 26`[1]`: once `File_LoadDone`, `Gfx_ClutStripRestore`, `Gfx_ClutStripDirty` 1, `0x939A3C..41` cleared, next |
| `0x456DF0` | `CommuTail_LoadSoundBank` | 14`[4]`, 23`[3] [12] [19]`, 26`[3]`: `0x90412C` bit 7 set, `Snd_LoadBankFile(low seven bits + 0x2C2)`, next |
| `0x456E20` | `CommuTail14_End` | `[5]`: once `File_LoadDone`, `Area131_DisarmTail`, `CommuTail_RestoreFacing`, R4A's `0x456080`, tail jump R4A's `0x455950` |
| `0x456E40` | `CommuTail_RestoreFacing` | `Sprite_Current` = the kept object; its `+8` = its `+0x85`; `Sprite_FaceDirection(+8)` |
| `0x456E70` | `CommuTail21_Dispatch` | `Field_ModeTailKinds[21]`: through `CommuTail21_States` (5) |
| `0x456E80` | `CommuTail_TimedGift` | 14`[6]`, 21`[0]`: the clock less the record's `+4` against a stack table of 20 `{bound, item, category}` (the last row whose bound it reaches); none: message 0x98; else the item's name into `Text_Records`, message 0x96, `Inventory_Add(category, item, 1)`: added, the record's `+4` = the clock and next; not added, the state after next. Field_Request 2 |
| `0x4570C0` | `CommuTail_EndAfterMessage` | 14`[7] [10]`, 23`[15]`: once Field_Request is 0, disarm, tail jump `CommuTail_RestoreFacing` |
| `0x4570E0` | `CommuTail_Open97` | 14`[8]`: once Field_Request is 0, message 0x97, the state before, Field_Request 2 |
| `0x457110` | `CommuTail22_Dispatch` | `Field_ModeTailKinds[22]`: through `CommuTail22_States` (2) |
| `0x457120` | `CommuTail_RandomGift` | 14`[9]`, 22`[0]`: by the record's `+2` - 3: message 0x52, the record kept (`+4` the clock, `+2` 0, the object's word `+0x88` = 0x51); 2: `Rand & 0x7F` (at most 100) counted down a 3-row stack table `{first, step, step, -}` picked by the record's slot's level (`+1`) to a tier 1..3, `Rand & 0xF` one of the tier's 16 `{item, category}` at `0x652A10`; `Inventory_Add`: not added message 0x50, added the name, message 0x4F and the record kept. Then next, Field_Request 2 |
| `0x4572F0` | `CommuTail23_Dispatch` | `Field_ModeTailKinds[23]`: through `CommuTail23_States` (21) |
| `0x457300` | `CommuTail23_LoadDat` | `[0]`: `Field_ScriptFlags2 |= 7`, `LoadDatFile(0x12D` for argument 3, else `0x12C)`, next |
| `0x457340` | `CommuTail_GameDispatch` | 23`[2]`: through `CommuTail_GameStates` by the u8 argument (16): its first four entries are R4C's / R4D's `0x459F20`, `0x45B770`, `0x45C8C0`, `0x45D040` |
| `0x457350` | `CommuTail_EndAfterLoad` | 23`[4] [13] [20]`: once `File_LoadDone`, `Field_ScriptFlags2 &= 0xFFF8`, disarm, tail jump `CommuTail_RestoreFacing` |
| `0x457370` | `CommuTail24_Dispatch` | `Field_ModeTailKinds[24]`: through `CommuTail24_States` (12) |
| `0x457380` | `CommuTail24_LoadDat` | 23`[9]`: flags `|= 7`, `LoadDatFile(0x12E)`, next |
| `0x4573B0` | `CommuTail25_Dispatch` | `Field_ModeTailKinds[25]`: through `CommuTail25_States` (7) |
| `0x4573C0` | `CommuTail_NibbleGift` | 23`[14]`: the record's `+3` high nibble picks a 6-byte row of `0x652AC4` `{message, message, count}`; count 0: the first message (nibble 0: state 2); else the name, `Inventory_Add(+3 & 0xF, +2, count)`: added, `+2` and `+3` cleared and the first message, else the second; next, Field_Request 2 |
| `0x4574E0` | `CommuTail25_LoadDat` | 23`[16]`: unless Field_Request is 2, flags `|= 7`, `LoadDatFile(0x12F)`, next |
| `0x457510` | `CommuTail26_Dispatch` | `Field_ModeTailKinds[26]`: through `CommuTail26_States` (5) |
| `0x457520` | `CommuTail26_LoadDat` | `[0]`: with `Field_ScriptFlags`, `0x904A90` and `0x937F80` all 0 the tail kind becomes 6; else flags `|= 7`, `LoadDatFile(0x130)`, next |
| `0x457570` | `CommuTail26_End` | `[4]`: once `File_LoadDone`, flags `&= 0xFFF8`, tail jump `Area131_DisarmTail` |
| `0x457590` | `CommuTail60_Stream` | `Field_ModeTailKinds[60]`: a switch on the s8 state - 0: the track kept in `0x939A3D`, `Music_FadeOutStop(10)`, `Sound_LoadStream(8`, or `0xA` with `0x939A3C` set`)`, next; 1: once `Sound_StreamDone`, message 0x2A1, next, Field_Request 2; 2: unless Field_Request is 2, `Music_Play(kept, 8)`, flags `&= 0xFFF8`, disarm, tail jump `CommuTail_RestoreFacing`; others nothing |

### 1.2 The board's states

| Address | Name | What it does |
|---|---|---|
| `0x457640` | `CommuBoard_Dispatch` | 14`[3]`: through `CommuBoard_States` by `0x939A3E` (14) |
| `0x457650` | `CommuBoard_Init` | `[0]`: the grid cursor mode 0, row 0, column 1; help 0xB5; next |
| `0x457680` | `CommuBoard_Grid` | `[1]`: cancel - help 0xFFFF, the tail's next state; confirm - the cursor kept, step and list cursor 0, allowed by mode (0 / 2: the slot holds a record; 1: the slot within the area's limit) sound 0x104, the slot's help, next, else sound 0x107; else the grid cursor moved on `0x9039F5`. Then the panel and the slot's lines, flashing |
| `0x457780` | `CommuBoard_ModeDispatch` | `[2]`: through `CommuBoard_ModeStates` by the kept mode `0x675F86` (11): mode 0 and 2 the Steps, 1 the StepsB |
| `0x457790` | `CommuBoard_StepDispatch` | `[3] [5]`: through `CommuBoard_Steps` by `0x939A40` (8) |
| `0x4577A0` | `CommuBoard_PickRecord` | Steps`[0]`: a record of the slot - cancel: sound 0x106, board state - 1, help 0xB5; confirm: sound 0x104, `0x675F7F` = the slot, next step, help 0x44; else the list cursor stepped (0x2000 / 0x8000, wrapping at the slot's count, sound 0x100). Then the panel, lines, R4E's hand at the cursor and the record's card |
| `0x457990` | `CommuBoard_MoveRecord` | Steps`[1]`: the target - cancel: sound 0x106, the slot's help, `CommuBoard_CancelStep`; confirm: `CommuBoard_PlaceRecord(0)`, placed help 0xB5; else the grid cursor and its help on `0x675F7F`. Then both slots' lines and, unless placed, the hand and the card |
| `0x457AD0` | `CommuBoard_StepDispatchB` | `[4]`: through `CommuBoard_StepsB` by `0x939A40` (6) |
| `0x457AE0` | `CommuBoard_PickRecordB` | StepsB`[0]`: as PickRecord over 0..count with sound 0x103 on confirm; at 0 the slot's kind is picked (`0x675F7F` = kind - 4, the lists reset, step 2). The hand at `CommuBoard_SlotRecordXY` and the card of the record before it |
| `0x457CE0` | `CommuBoard_MoveRecordB` | StepsB`[1]`: as MoveRecord with `PlaceRecord(1)`, cancel CancelStep then the help |
| `0x457DD0` | `CommuBoard_PickList` | Steps`[4]`, StepsB`[2]`: the first list (`0x675F7F` over `0x9046CB` rows; help `0x652AF8[row]`); confirm: the next list `0x652B44[row]` - 0xFF: R4C's `0x459EE0`; else `0x675F7E` from the slot when the row is its kind, `0x675F83` = it, help `0x652B50[it]`, next step. Cancel: the slot's help, step 0 |
| `0x457FE0` | `CommuBoard_PickListB` | Steps`[5]`, StepsB`[3]`: the second list (`0x675F7E` over `0x652B58[2 * list]`); confirm: `0x652B59[2 * list]` - 0xFF R4C's `0x459EE0`; else `0x675F7D` from the slot, `0x675F82` = it, help 0x46, next |
| `0x458240` | `CommuBoard_PickListC` | Steps`[6]`, StepsB`[4]`: 0x1000 / 0x4000 flips `0x675F7D`; confirm R4C's `0x459EE0`; cancel help 0x45 |
| `0x458410` | `CommuBoard_Confirm` | Steps`[7]`, StepsB`[5]`: the yes / no row - yes: unless the slot already holds the picks, every record whose `+1` is the slot (in use or not) restarts (`+4` the clock, `+2` `+3` 0) and so does the slot; the slot takes kind `0x675F7F + 4`, `+1` the first list's row, `+2` the toggle; the grid cursor back, sound 0x104, board state - 1, help 0xB5. No, or cancel: sound 0x106, the help and step R4C's ask kept. Else 0x2000 / 0x8000 flips the row |

### 1.3 The draws and helpers

| Address | Name | What it does |
|---|---|---|
| `0x458830` | `CommuBoard_DrawRecordCard` | `(x, y, record, highlight)`: shadow and box, the frame, the record's 5-byte name `0x9048F0 + 5 * record`, a black tile 60 x 16, four bars of 12 times the record's bytes `0x653210 + 20 * record`; with highlight, the bar `0x652B60[k]` names flashes (k: the slot `0x675F7F` - 0, slot - 8 from 9, else the slot's kind) |
| `0x458AE0` | `CommuBoard_DrawCardFrame` | `(x, y)`: the card's frame of board sprites |
| `0x458B90` | `CommuBoard_DrawBar` | `(x, y, w, row, flash)`: two shaded quads; the colour row of a 4-row stack table times 0x80 or the frame's flash (an 8-bit signed multiply), dim `>> 2` |
| `0x458D70` | `CommuBoard_DrawPanel` | the title box, the help line (and for help 0x3D a flashing translucent tile at the yes / no row), the panel, the slot-0 row, the slots 9..11, the eight slots (locked: a box; else sprite 0, the kind's number `CommuBoard_DrawDigits`, a sprite per record), two counters (`Crt_sprintf`, `Text_DrawFont12`) beside a label |
| `0x4591A0` | `CommuBoard_DrawPanelFrame` | `(x, y)`: the panel's frame of board sprites |
| `0x459250` | `CommuBoard_DrawSlotLines` | `(slot, steady)`: the slot's two three-point lines from its row `0x652B7C` (six s16), (I, I, 0) with I 0xFF or the flash; a locked slot starts at other points |
| `0x459430` | `Commu_CountInSlot` | `(slot)` -> al: records in use in the slot |
| `0x459460` | `Commu_NthInSlot` | `(slot, nth)` -> eax: the nth such record's index, 0xFF when fewer |
| `0x4594A0` | `CommuBoard_MoveGridCursor` | `(cell)`: the grid cursor at `cell` moved by `Input_AutoRepeat(pressed & 0xF000)` through the three modes; sound 0x100 for any of the four keys |
| `0x459720` | `CommuBoard_DrawDigits` | `(x, y, n)`: a 16 x 16 sprite of the number n |
| `0x4597E0` | `CommuBoard_DrawSprite` | `(x, y, id)`: the board sprite from its 6-byte row `0x652C18` |
| `0x4598A0` | `CommuBoard_DrawListBox` | `(x, y, steady)`: the first list's box, `0x9046CB` lines from `0x669E68` |
| `0x459960` | `CommuBoard_DrawListFrame` | `(x, y, h)`: a list's frame; `(h >> 3) - 1` side rows as a byte |
| `0x459A30` | `CommuBoard_ListY` | -> eax: the first list's y |
| `0x459A80` | `CommuBoard_DrawListBoxB` | `(x, y, list, steady)` -> ax: list's box from `0x652C6C[list]` `{first, count}`, moved up to stay above 0xD8; answers the y drawn at |
| `0x459B70` | `CommuBoard_SlotRecordXY` | `(short *x, short *y, slot, k)`: the slot's corner, or its record position k from `0x652C74` |
| `0x459C40` | `CommuBoard_CancelStep` | the grid cursor put back, sound 0x106, step - 1 |
| `0x459C80` | `CommuBoard_PlaceRecord` | `(back)` -> al: the record at the list cursor less `back` moved to `0x675F7F` unless refused (mode 1: the target holds three or is past the limit; a mode past 2; its own slot) |
| `0x459D80` | `CommuBoard_SlotHelp` | the slot's help: 0x6A, `2 * slot + 0x2C` from 9, `+1 + 0x4A` for kind 4, a 9-word stack table for kinds 5..13 |
| `0x459E50` | `CommuBoard_PickHelp` | `(cell)`: the help for the slot at `cell` |

## 2. The extents, the starts

The band tool's read extents (`band_rows.py --byte-tables`, run through the
scratch wrapper `band14.py` - the plain tool did not stop on this group) are
the ones above; the cut's sizes differ by padding only (35 rows, 0 by code).
`0x459E50` ends at `0x459ED9` (`ret`), before R4C's `0x459EE0`. Every one of
the 60 starts is a function with its own `ret` or tail jump: none is a case
of a host, and no shared tail was found - the tail jumps (`0x456E20` to
`0x455950`, `0x4570C0` / `0x457350` / `0x457590` to `0x456E40`, `0x457570` to
`Area131_DisarmTail`) are taken as calls. The band holds no code the cut does
not list.

Hidden starts: 39 (the catalog's `hidden` column). The catalog put
`0x456D50..0x456E20` in R4A's `0x456AF0` and `0x456E70..0x458410` in this
group's `0x456E40`; both are runs of separate functions reached through the
tables of section 3, not one host. `entries_logic.txt` carries
`00456AF0 34E` (R4A's line, covering six of ours) and `00456E40 19E3` (the
run from `0x456E40` to `0x458822`, where `CommuTail_RestoreFacing` is 0x24
bytes): both left as they are for the round's end (a second `00456E40` line
would be a duplicate); the 39 hidden starts have their own lines now.

## 3. The state tables

The dispatchers read four runs of code words, each from several cells (a
table's tail is another table). Each is named from the cell its reader reads,
with the count to the end of its run (the readers bound nothing). The fuzz
lists the four runs once each (`DataTable`), which covers the tails.

| Table | Count | Read by | Entries |
|---|--:|---|---|
| `CommuTail14_States` `0x6529E4` | 11 | `CommuTail14_Dispatch`, s8 `0x9039F4` | `0x456D60 0x456D90 0x456DB0 0x457640 0x456DF0 0x456E20 0x456E80 0x4570C0 0x4570E0 0x457120 0x4570C0`, then data |
| `CommuTail21_States` `0x6529FC` | 5 | `CommuTail21_Dispatch` | `CommuTail14_States[6..10]` |
| `CommuTail22_States` `0x652A08` | 2 | `CommuTail22_Dispatch` | `[9..10]` (then `0x652A10`, RandomGift's pairs) |
| `CommuTail23_States` `0x652A70` | 21 | `CommuTail23_Dispatch` | `0x457300 0x456DB0 0x457340 0x456DF0 0x457350`, R4C/R4D/R4E's `0x459F20 0x45B770 0x45C8C0 0x45D040`, `0x457380 0x456DB0`, `0x45F750`, `0x456DF0 0x457350 0x4573C0 0x4570C0 0x4574E0 0x456DB0`, `0x45FE00`, `0x456DF0 0x457350`, then data |
| `CommuTail_GameStates` `0x652A84` | 16 | `CommuTail_GameDispatch`, u8 `0x9039F5` | `CommuTail23_States[5..20]` |
| `CommuTail24_States` `0x652A94` | 12 | `CommuTail24_Dispatch` | `[9..20]` |
| `CommuTail25_States` `0x652AA8` | 7 | `CommuTail25_Dispatch` | `[14..20]` |
| `CommuTail26_States` `0x652AE4` | 5 | `CommuTail26_Dispatch` | `0x457520 0x456DB0`, R4E's `0x460500`, `0x456DF0 0x457570`, then data |
| `CommuBoard_States` `0x652B0C` | 14 | `CommuBoard_Dispatch`, u8 `0x939A3E` | `0x457650 0x457680 0x457780 0x457790 0x457AD0 0x457790 0x4577A0 0x457990 0x457AE0 0x457CE0 0x457DD0 0x457FE0 0x458240 0x458410`, then data |
| `CommuBoard_ModeStates` `0x652B18` | 11 | `CommuBoard_ModeDispatch`, u8 `0x675F86` | `CommuBoard_States[3..13]` (the kept mode is 0..2 in play) |
| `CommuBoard_Steps` `0x652B24` | 8 | `CommuBoard_StepDispatch`, u8 `0x939A40` | `[6..13]` |
| `CommuBoard_StepsB` `0x652B2C` | 6 | `CommuBoard_StepDispatchB`, u8 `0x939A40` | `[8..13]` |

`Field_ModeTailKinds` `0x662CE8` (64, already named) holds the seven tail
dispatchers at 14, 21..26 and 60; no code of this group reads it. Constant
tables the code indexes are read in place (the callees header names them:
`0x652A10`, `0x652AC4`, `0x652AF0`, `0x652AF8`, `0x652B44`, `0x652B50`,
`0x652B58`, `0x652B60`, `0x652B6C`, `0x652B7C`, `0x652C0C`, `0x652C18`,
`0x652C6C`, `0x652C72`, `0x652C74`, `0x653210`, `0x653601`, `0x669E68`,
`0x669EE0`); none is named as `[[data]]` - their extents are the indexes'
reach, not read here.

## 4. The fuzz (`rest_4b_fuzz.cpp`)

Field mode, 6,000 rounds a function. The 60 clones from
`band_rows.py --clones`; the states and dispatchers `kState`, the draws and
helpers `kCall` with their arguments set by `Args` (coordinates at boundaries,
records below 60 mostly, the colour row below 4, the cell pointers
`0x9039F5` / `0x675F7F` for MoveGridCursor and PickHelp, scratch pointers for
SlotRecordXY's two words). Each dispatcher's index byte is seeded below its
table's count; the four runs are `DataTable`s.

**Seeds** (every round): the buttons (a confirm bit, a cancel bit, the pressed
word on either, both or none); the 60 records (in use or not, a slot 0..11
mostly, the marks 0 / 2 / 3, a time); the eight slots (kind 0 or 4..13
mostly, a level 0..2); the tail's kind, state and argument; the kept object
one of the first four sprites; the board's bytes, cursor, picks (0xFF often
for the open lists), the help word (0x3D often), the list count, the area
number (0xAF..0xB9 often), Field_Request, the message index (0xF9 often), the
three busy bytes, and the clock at and beside each bound of TimedGift's table
from the argument's record. Per clone: RandomGift's records all in slots 1..8
(their levels below 3, since the disturbance moves the argument); TimedGift's
and NibbleGift's argument below 60; Confirm's argument 1..8 and the picks the
slot's own half the time; SlotHelp's slot kind 0 or 4..13; PlaceRecord's and
PickHelp's target the source half the time; MoveGridCursor's two cells at the
cursor's boundaries.

**Stand-ins** (every callee listed in the group, registered before the
standard rows): ours by name with what each reads - the draws' coordinates as
words, bytes where the callee reads a byte (`Item_NamePtr`'s two, the
records', slots' and colour rows'); `SlotRecordXY`'s pointers unlogged (the
two sides' stacks) and its stand-in writing the same two words through them;
`Input_AutoRepeat` answering each key the states test, alone and together;
`Item_NamePtr` answering into the harness's text buffer; `Commu_CountInSlot`
0..4, `Commu_NthInSlot` 0..59 with garbage above; the other groups' four by
address. **The group's disturbance** (`Move`, 15 cases by
`sh::DisturbCase(h, 15)`, every value from `h`): the tail state, the argument
(1..8), the board state and step, the list cursor, the pick and the first
row, the open lists, the help word, a slot's byte (kind 0 / 4..13, level
0..2), a record's slot (1..8) or marks, the list count, the clock, the toggle
or `0x675F7D`, the grid cursor's bytes. It runs from the harness's disturbance
and from most stand-ins (`Stir`).

**What it lacks** (for the coordinator's fold, not edited here): the harness
has no shape for a handler that is also reached by a table and by a tail jump
(the tail states are fuzzed as `kState` only); `Gpu_SetLineF3` and
`Gpu_SetSprt16` are effect-mode standard rows only, so a field-mode group
lists them.

## 5. Latent defects and ranges (Capcom's, described, not fixed)

One policy for the group: **a write past its table, a read of a stack table
past its end, or a jump past a run aborts loudly**; reads of `.data` tables by
an index the code computes read the image in place. Each abort's message
names what the original does instead.

- **The dispatchers** jump through their runs by an unchecked byte (s8 for the
  tail state, u8 for the board's): past the run they jump through data.
  Ours aborts outside the counts of section 3. Every state writes an index
  inside its run.
- **`CommuTail_TimedGift`, `_RandomGift`, `_NibbleGift`** write the record the
  argument names, for any argument byte: past the 60 records they write on
  into the save block (`0x904ED0` at most). Ours aborts past 60. The argument
  is set by R4A's arming handlers from an object's byte; not read here.
- **`CommuTail_RandomGift`** reads its 3-row stack table by the record's
  slot's `+1` (a level): a level of 3 or more reads the return address and
  the caller's frame as the row. Ours aborts. The slots' levels are written by
  `CommuBoard_Confirm` from the first list's row.
- **`CommuBoard_DrawBar`** reads its 4-row colour table by `row`: its one
  caller passes 0..3.
- **`CommuBoard_SlotHelp`** reads its 9-word stack table by kind - 5: a kind
  of 1..3 or past 13 reads past it (uninitialised words, then the frame).
  Ours aborts. A slot's kind is `0x675F7F + 4` from Confirm: the first list's
  row (0..`0x9046CB` - 1) plus 4.
- **`CommuBoard_PlaceRecord`** writes the record `Commu_NthInSlot` answers;
  0xFF (the list cursor past the slot's records) writes record 255
  (`0x904EC8..0x904ECF`). Ours aborts. The callers' cursors stay inside the
  slot's count.
- **`CommuBoard_Confirm`** writes the slot the argument names: the argument is
  1..8 on its path (the kept mode 1, section 1.2); 0 or past 8 would write a
  record's or a name's bytes. Ours aborts.
- **`CommuBoard_Confirm` restarts unused records too**: the loop compares
  every record's `+1` with the slot without testing `+0` (in use), so a freed
  record that last sat in the slot gets its time and marks reset. Faithful.
- Reads in place: `CommuBoard_PickRecordB`, `DrawRecordCard`, `PickList` /
  `PickListB` read `0x9048A8 + 8 * slot` for slot 0 (record 59's bytes) and
  for a negative s8 pick; `DrawSlotLines` reads its row for any slot byte;
  `PickHelp` reads `0x652AF0[kind]` and `0x652C72[slot]` for any byte.

No full-frame fill in the band (DIV-0041 untouched); no widescreen, cheat or
ledger patch inside it. **Needs a ledger entry: none.**

## 6. Controls

CONTROLS_SECTION

## 7. Calls across groups

Out of the group (raw, by address in `rest_4b_callees.h`, until the round's
rebinding): R4A's `0x456080` and `0x455950` (from `CommuTail14_End`), R4C's
`0x459EE0` (3 sites: the lists' 0xFF next and PickListC's confirm), R4E's
`0x45ECC0` (11 sites: the hand). R2B's `Menu_DrawOutlineNotched` `0x57D520`
(4 sites) is merged and called by name. The tables hold other groups'
functions: `CommuTail23_States` R4C/R4D/R4E's `0x459F20`, `0x45B770`,
`0x45C8C0`, `0x45D040`, `0x45F750`, `0x45FE00`; `CommuTail26_States` R4E's
`0x460500` - entries, not calls; they stay as the image has them.

Inbound from outside the group: none by call. `Field_ModeTailKinds[14, 21..26,
60]` holds the seven dispatchers (the field's tail runner reads it); R4C's
`0x459EE0` writes `0x675F78`, `0x675F7A`, `0x675F7C`, `0x675F81` and
`0x939A40` = 5 (`CommuBoard_StepsB[5]` is the Confirm step; on the Steps path
step 5 is `CommuBoard_PickListB` - not read further here), which this group
reads.

## 8. The rebinding

`band_rows.py --refs`: 0 raw references to the 60 in `src/game`. Nothing to
rebind; no harness row lists any of the 60 (`scenario_harness*.cpp`,
`boss_harness*.cpp` grepped). `area_w3c.cpp` line 461 names call sites in the
band in a comment (`0x456E29 .. 0x4575CE`, the tail phases calling
`ScriptFlags_Clear40`'s neighbours): text, not a reference.

## 9. The live route

None: no recorded route enters the faerie village, and the catalog shows no
row of this group reached. Fuzz only; the owner records a route once the code
is ours.

## 10. Self-tests and the entry list

SELFTEST_SECTION
