# Group R4D: two of the community's games (CommuDraw, CommuName) and the board R4C draws with

**Status:** MEASURED (2026-10-05) - round fourteen
([`takeover-queue-round14.md`](takeover-queue-round14.md)), wave four, from the
round branch's tip `35ec19ef`. **60 functions ours** (`src/game/rest_4d.cpp`,
declarations in `src/game/rest_4d.h`, the tables' and cells' addresses in
`src/game/rest_4d_callees.h`, shadow name `rest_4d`): the cut's 60 rows for R4D
(`analysis/round14_cut.tsv`), no start dropped or added (section 2); each read
to its last instruction with capstone and fuzzed through the scenario harness
in field mode ([`scenario_harness.md`](scenario_harness.md) section 7), used
unchanged: 240,000 rounds, 0 mismatches. **173 controls planted** one at a time: 170 refused by a count, 3 equivalent mutants not refused, each with a near variant refused (section 6). No recorded
route enters the community (section 9): **fuzz only**.

The band is three things. **The board** (`0x45C400..0x45C8B0`): four draw
helpers R4C's states call (33 sites) - eight rows of the five-byte records at
`0x675F98`, R4C's cells and frame pieces. **CommuDraw** (`0x45C8C0..0x45D03F`):
entry 7 of R4B's table `0x652A70` (R4B's `0x4572F0` jumps through it by
`0x9039F4`) - a draw of names at random out of `MessagePools` into the nine
`Text_Records`, a title line built from two more, a reveal, two music changes
and message 0xE2. **CommuName** (`0x45D040..0x45E86E`): entry 8 - a five-byte
name for one of 60 slot cells (`0x9046D0`, names at `0x9048F0`) or for one of
the seven character records with bit 0 of `+0xB` set, either drawn at random
(R4E's `0x45ED70`) or entered. On the PC the entry is gone: its draws are
calls of `BareRet` with the PlayStation's arguments, and its answer
`BareRetZero` is always 0, so the original never leaves the entry's step and ours ends it in message 0xF7 and state 4 (DIV-0075)
(section 5, L3). The two games share `Commu_LeaveWhenClosed` `0x45E6A0` (the
last state of both tables and of two of R4E's).

**What the screens are in play is not stated here.** The cut's class (COMMU,
the faerie village's overlays by the project's memory) and the code are the
only sources; the names say what the code does. The PSX twins
(`analysis/pairs_propagated.json`): `0x45C400` - `0x801D4A98`, `0x45C7D0` -
`0x801D4E5C`, `0x45C850` - `0x801D4F40`, all in COMMU02 (`5434a7fa...`,
call-anchored); the sibling's `names/` and `symbols.toml` name none of them
(COMMU02's only table there is the roster names, `names/characters.toml`).
Nothing is transferred.

## 1. What each function does

### 1.1 The board (R4C's states; `0x675F98` five bytes a row, `0x675F8C` the row)

| Original | Name | What |
|---|---|---|
| `0x45C400` | `CommuBoard_DrawRows(x, y, shown)` | `Menu_DrawBox` (x, y) 0x52 x 0x7A in the window colour, the frame, then eight rows 14 apart; row k's bytes at `0x675F98 + (5k + 3) & 0xFF`. Below the cursor (s8 `0x675F8C`, read again each row), and at it when `shown`'s byte is not 0: three bytes in one `Crt_sprintf` line (`0x653088`, three numbers) in Font12 colour 0, then the fourth and fifth as numbers (`0x5E10C0`) in colours 2 and 1; at it with `shown` 0 the blank formats (`0x653094`, `0x653090`) in 0 / 2 / 1; past it the blanks in colour 7. Each row then `0x653084` in Font8 at (x + 0x3B, y + 14k + 9), colour 7 past the cursor, else 0 |
| `0x45C700` | `CommuBoard_DrawFrame(x, y)` | one draw-mode packet (tpage 0x1D) committed, then R4C's `0x45B400` pieces: 0xB at (x, y), nine 0xD at y - 1, 0xC at x + 0x4F; fourteen rows of 0xE / 0xF at x and x + 0x50; 0x10, nine 0xD and 0x11 at y + 0x78 |
| `0x45C7D0` | `CommuBoard_DrawRowCells(x, y, row, lift)` | the row's first three bytes as R4C's `0x45B2C0` cells 32 apart; cell i 8 higher when s8 `0x675F8D` (read again each cell) equals i and `lift`'s byte is not 0 |
| `0x45C850` | `CommuBoard_DrawCells(x, y, shown)` | `0x675F98..9A` as three cells, or three cells of 0xFF when `shown`'s byte is 0 |

### 1.2 CommuDraw (`CommuDraw_States` `0x652DC4` by `0x939A3E`; steps by `0x939A40`)

| Original | Name | Table entry | What |
|---|---|---|---|
| `0x45C8C0` | `CommuDraw_Dispatch` | `0x652A70[7]` | `jmp [CommuDraw_States + 4 * 0x939A3E]` |
| `0x45C8D0` | `CommuDraw_OpenStep` | state 0 | `jmp [CommuDraw_OpenSteps + 4 * 0x939A40]` |
| `0x45C8E0` | `CommuDraw_FadeOut` | 0.0 | `Transition_Start(0)`, the step on |
| `0x45C900` | `CommuDraw_MusicIn` | 0.1 | once the wait word is 0: `Music_Track` kept at `0x675F90`, `Draw_PassFlags` 0, `Music_FadeOutStop(10)`, `Music_Play(0x67, 8)`, state 1 step 0 |
| `0x45C950` | `CommuDraw_ShowStep` | state 1 | `jmp [CommuDraw_ShowSteps + 4 * 0x939A40]` |
| `0x45C960` | `CommuDraw_Pick` | 1.0 | `Rand & 7` a row into `0x675F8C`, the four counts `0x675FC0..C3` cleared; for each of the nine text records the row's next category byte of the 8 x 9 table `0x652DEC` (9 ends): a pick `CommuDraw_RandBelow(count)` (the category's first message and count are the pair at `0x652E34`), moved on while every earlier pick of the category equals it (L1), kept in the category's ten at `0x675F98 + 10c`, counted; the message (first + pick, `MessagePools` word `0x803780 + 2m`) copied into the record - each byte, ending after a 0 whose byte before has no bit 7, or at 0x20. Then the title's two messages `0x675FC5` / `0x675FC6` (0xFF none) from a row of `0x652E3C` chosen by `Rand & 7` against its thresholds (row 6 turns 2 into 3, row 7 3 into 2), each one of the picks or a fresh `RandBelow` by `Rand`'s bit 0. The step on, `0x675F95` 0 |
| `0x45CC50` | `CommuDraw_Reveal` | 1.1 | the title; a semi-transparent POLY_G4 (abr 2) black at y = `0x675F95` to white at y + 0x10, x 0..320, and a black TILE from y + 0x10, 320 wide, 0xE0 - y high; `0x675F95` + 1, + 3 while `Input_Held`; the step on at 0xC8 |
| `0x45CDC0` | `CommuDraw_WaitKey` | 1.2 | the title; on any `Input_Pressed`, `Transition_Start(0)` and the step on |
| `0x45CDF0` | `CommuDraw_MusicBack` | 1.3 | while the wait word is not 0 the title (a tail `jmp`); then `Music_FadeOutStop(10)`, `Music_Play(0x675F90, 8)`, `Draw_PassFlags` 0x1F, `Transition_Start(1)`, the step on |
| `0x45CE40` | `CommuDraw_Close` | 1.4 | once the wait word is 0: message 0xE2, `Field_Request` 2, state 2 step 0 |
| `0x45CE80` | `CommuDraw_RandBelow(limit)` | - | al: `Rand & 0x7F`, less `Rand & 0xF` (bytes, wrapping) while not below the limit's byte (L2) |
| `0x45CEA0` | `CommuDraw_DrawTitle` | - | at the text scratch `0x904BA0`: `*`, messages `0x675FC5` and `0x675FC6` (unless 0xFF) of the words `0x803780` (a byte with bit 7 copies the next one with it), `+`, a space, the five name bytes of the slot named by the byte `+5` of the record `0x939A38` points at, 0; `Text_DrawAt` (0x32, 0x1E); then the `MessagePools` word `0x803746 + 2 * s8 0x675F8C` (read after the first draw) at (0x32, 0x42) |

`Commu_LeaveWhenClosed` `0x45E6A0` is state 2: once `Field_Request` is not 2,
`0x9039F4` one on (R4B's table moves to its next game).

### 1.3 CommuName (`CommuName_States` `0x652E48` by `0x939A3E`)

State 0 `CommuName_Begin` (message 0xEE); state 1 the slots, state 2 the
records, each a nine-step table by `0x939A40` whose steps 6 and 7 dispatch
again by `0x939A3F`; state 3 `CommuName_End` (message 0xF6 once at step 0);
state 4 `Commu_LeaveWhenClosed`. State 3 is written by R4E's `0x45F5A0` /
`0x45F650` (the new name written, then state 3); state 2 by no function of this
group. `0x675F95` is the panels' slide (4..0 in, 0..4 out), `0x675F92` the
header's message (0xF0, 0xF1, 0xFFFF none), `0x675F8C` the cursor, `0x675F8D`
the yes / no answer.

| Original | Name | Step | What |
|---|---|---|---|
| `0x45D040` | `CommuName_Dispatch` | `0x652A70[8]` | `jmp [CommuName_States + 4 * 0x939A3E]` |
| `0x45D050` | `CommuName_Begin` | state 0 | unless a message is open: message 0xEE, `Field_Request` 2, state 1 |
| `0x45D080` | `CommuName_SlotStep` | state 1 | `jmp [CommuName_SlotSteps + 4 * 0x939A40]` |
| `0x45D090` | `CommuName_PanelReset` | 1.0, 2.0 | unless a message is open: sound 0x102, the cursor 0, the slide 4, the header 0xF0, the step on |
| `0x45D0D0` | `CommuName_SlotPanelIn` | 1.1 | the slide down; the title box (`Menu_DrawPanelBox` (0x14, 0x10 - 8 * slide)); R4E's `0x45E870` slot panel at 0x44 - 30 * slide; the slot bar at (0x4A, 0x64 - 30 * slide); the step on at 0 |
| `0x45D170` | `CommuName_SlotChoose` | 1.2 | confirm: header 0xF1, sound 0x104, the step on, the answer 0; cancel: sound 0x106, step 8; else `Input_AutoRepeat(pressed & 0xA000)` (sound 0x100 on any) moves the cursor up / down over `CommuName_CountSlots`, wrapping; the header, the panel, the bar, R4E's `0x45ECC0` hand at 0x55 + 8 * cursor |
| `0x45D290` | `CommuName_SlotConfirm` | 1.3 | the ask: confirm on answer 1 - header 0xF0, sound 0x104, step back; on 0 - header 0xFFFF, sounds 0x104 and 0x102, step on, slide 0; cancel - header 0xF0, sound 0x106, step back; else a repeat flips the answer (sound 0x100). While the header shows: it, `Menu_DrawHand` at 0xCE + 36 * answer when it is 0xF1, the bar and the hand (flag 1); the panel always |
| `0x45D3E0` | `CommuName_SlotPanelOut` | 1.4 | the slide up; the box, the bar at 0x64 + 40 * slide, the panel; at 4 message 0xF2, the step on |
| `0x45D480` | `CommuName_SlotHow` | 1.5 | once the message is closed: step 6 when `0x939A3C` is 0, else the slide 5 and step 7; the panel |
| `0x45D4D0` | `CommuName_SlotRandomStep` | 1.6 | `jmp [CommuName_SlotRandomSteps + 4 * 0x939A3F]` |
| `0x45D4E0` | `CommuName_SlotRandomPick` | 1.6.0 | R4E's `0x45ED70` (a name at `0x675F98`, answering its length + 1) copied into `Text_Records` by `rep movs`; message 0xF3, `0x939A3F` on; the panel |
| `0x45D550` | `CommuName_SlotRandomAsk` | 1.6.1 | once the message is closed: `0x939A3D` not 0 - sound 0x102, on, slide 0; else back to 1.6.0; the panel |
| `0x45D5B0` | `CommuName_SlotRandomOut` | 1.6.2 | the slide up, the panel at 0x44 - 30 * slide; at 4 a tail `jmp` to R4E's `0x45F650` |
| `0x45D600` | `CommuName_SlotEntryStep` | 1.7 | `jmp [CommuName_SlotEntrySteps + 4 * 0x939A3F]` |
| `0x45D610` | `CommuName_SlotEntryIn` | 1.7.0 | the panel; the entry draws (`BareRet(0x675FCB, 0xB0 + 30s, 0x2B)`, R4E's `0x45F1A0(0x12, 0x61 + 30s, 0x24, 0x11)`, `BareRet(0x675FCB, 0x18, 0x67 + 30s, 0x675FCC, 0x675FCA)`, s the slide read before each); the slide down; at 0 the slot's five name bytes (0 read as a space) into `0x675F98`, on, the column `0x675F94` and the three bytes 0 |
| `0x45D730` | `CommuName_SlotEntry` | 1.7.1 | the panel (flag 1), `Menu_DrawGreyHLine(0x4A + 12 * column, 0x42, 0xC, 0)`, the entry draws at 0, `0x675F96 = BareRetZero()` (0) |
| `0x45D7C0` | `CommuName_SlotEntryOut` | 1.7.2 | the slide up; the panel at 0x44 (answered) or 0x44 - 30 * slide; the entry draws; at 4, answered: an empty name (`0x675F98` 0) takes the slot's default `0x653200 + 20 * slot` (read in place), message 0xF4, on; unanswered: message 0xF7, state 4, step 0 |
| `0x45D930` | `CommuName_SlotEntryAsk` | 1.7.3 | the panel (flag 1); once the message is closed: `0x939A3D` 0 - slide 0, on; else slide 4, back to 1.7.0 |
| `0x45D990` | `CommuName_SlotEntryDone` | 1.7.4 | as 1.6.2 with flag 1 |
| `0x45D9E0` | `CommuName_SlotClose` | 1.8 | the slide up; the box, the panel at 0x20 - 30 * slide, the bar; at 4 message 0xF7, state 4, step 0 |
| `0x45DAA0` | `CommuName_MemberStep` | state 2 | `jmp [CommuName_MemberSteps + 4 * 0x939A40]` |
| `0x45DAB0` | `CommuName_MemberPanelIn` | 2.1 | the slide down; the box; R4E's `0x45EE10(x, y, record, 0)` for each of the seven records with `+0xB` bit 0 (read again after each panel), the n-th at row n >> 1 (y = 0x2B + 56 row), x 0x20 - 40 * slide or 0xA8 + 40 * slide by n & 1; the step on at 0 |
| `0x45DB90` | `CommuName_MemberChoose` | 2.2 | confirm / cancel as 1.2 (cancel also sound 0x102 and slide 0); else `Input_AutoRepeat(Input_Pressed & 0xF000)` (the word read again after R4E's `0x45F000`): left / right flips the column unless past the last record, up two back (from the top: `(cursor | 0xFE) & last`), down two on (past `(cursor | 0xFE) & last`: the top of the column); R4E's `0x45EF90` and `0x45F050` hand (flag 1) at (0x20 + 136 * column, 0x2B + 56 * row); the header |
| `0x45DD10` | `CommuName_MemberConfirm` | 2.3 | the ask as 1.3; then `0x45EF90` and the hand (flag 0) always |
| `0x45DE60` | `CommuName_MemberPanelOut` | 2.4 | the slide up; the box; the records' panels sliding out, the cursor's moved toward (0x20, 0x2B): x = 34 * column * (4 - slide) + 0x20, y = ((14 * row) & 0xFF) * (4 - slide) + 0x2B; at 4 message 0xF2, on |
| `0x45E000` | `CommuName_MemberHow` | 2.5 | as 1.5 (the slide 4 for step 7); R4E's `0x45EE10(0x20, 0x2B, 0x45F020(cursor), 0)` |
| `0x45E050` | `CommuName_MemberRandomStep` | 2.6 | `jmp [CommuName_MemberRandomSteps + 4 * 0x939A3F]` |
| `0x45E060`, `0x45E0D0` | `CommuName_MemberRandomPick`, `_MemberRandomAsk` | 2.6.0, 2.6.1 | as 1.6.0, 1.6.1 with the member's panel |
| `0x45E130` | `CommuName_MemberRandomOut` | 2.6.2 | the slide up, the panel at 0x20 - 40 * slide; at 4 a tail `jmp` to R4E's `0x45F5A0` |
| `0x45E180` | `CommuName_MemberEntryStep` | 2.7 | `jmp [CommuName_MemberEntrySteps + 4 * 0x939A3F]` |
| `0x45E190` | `CommuName_MemberEntryIn` | 2.7.0 | the slide down (before the panel); the panel; the entry draws; at 0 the record's first five bytes (0 as a space) into `0x675F98`, on, the four entry bytes 0 |
| `0x45E2C0` | `CommuName_MemberEntry` | 2.7.1 | the panel (flag 1), the grey line at (0x5B + 12 * column, 0x50), the entry draws, `0x675F96 = BareRetZero()` |
| `0x45E350` | `CommuName_MemberEntryOut` | 2.7.2 | as 1.7.2, the panel at 0x20 (answered) or 0x20 - 30 * slide, an empty name taking the five bytes `CommuName_RecordNames[record]` points at |
| `0x45E4C0`, `0x45E520` | `CommuName_MemberEntryAsk`, `_MemberEntryDone` | 2.7.3, 2.7.4 | as 1.7.3, 1.7.4 with the member's panel (the latter's tail `jmp` to `0x45F5A0`) |
| `0x45E570` | `CommuName_MemberClose` | 2.8 | the slide up; the box; the panels at y 0x30 + 56 row, x 0x18 - 40 * slide or 0xA8 + 40 * slide; at 4 message 0xF7, state 4, step 0 |
| `0x45E670` | `CommuName_End` | state 3 | once the message is closed and at step 0: message 0xF6 |
| `0x45E6A0` | `Commu_LeaveWhenClosed` | state 4 | above |
| `0x45E6B0` | `CommuName_CountSlots` | - | al: the cells `0x9046D0..0x9048AF` (8 bytes) whose first byte is not 0 (the original clears al alone; every caller reads al) |
| `0x45E6D0` | `CommuName_NthSlot(n)` | - | eax: the index (0..59) of the cell in use with n's byte in use before it; 0xFF none |
| `0x45E700` | `CommuName_DrawSlotBar(x, y)` | - | `Menu_DrawBox` (x, y) 0xAC x 0x1A flags 0x80, the frame, R4E's `0x45EC00` piece 1 at (x + 8i + 7, y + 9) for each slot in use |
| `0x45E770` | `CommuName_DrawSlotFrame(x, y)` | - | R4E's pieces: 6, twenty of 7 along y and 0xC along y + 0x16, 8 at x + 0xA8, two rows of 9 / 0xA at y + 8, y + 0xE, 0xB and 0xD |
| `0x45E820` | `CommuName_DrawHeader` | - | the title box at (0x14, 0x10); unless the header word (read after it) is 0xFFFF, its `MessagePools` message by `Text_DrawAt` at (0x1B, 0x13) |

## 2. The extents, the starts

`band_rows.py --byte-tables --group R4D` (through the scratch `band14.py`:
the tool itself stops at "settle: no fixpoint in 8 rounds") reads 8,872 bytes
where the cut says 9,243: every difference is the `nop` padding after a `ret`
(49 rows), none by code; every extent was read again here to its last
instruction. **All 60 are functions**: the 49 hidden starts are each reached
by a `.data` cell (`0x652A70`, `0x652DC4..`, `0x652E48..`) and each has its own
`ret` (the ten dispatchers are a `jmp` through a table). No start is a case or
a shared tail; the band has no code no list has. Tail calls: `CommuDraw_MusicBack`
`jmp`s to `CommuDraw_DrawTitle`; four states `jmp` to R4E's `0x45F5A0` /
`0x45F650` - each a call in ours (`disable_tail_calls`).

## 3. The state tables

Every dispatcher is `xor eax, eax; mov al, [state]; jmp [eax * 4 + T]`,
unchecked. The tables sit back to back and overlap no code; a reader's
index past its table's count runs into the next table. Each count is the
states its game writes, read by hand:

| Table | Name | Count | By | Entries |
|---|---|--:|---|---|
| `0x652DC4` | `CommuDraw_States` | 3 | `0x939A3E` | OpenStep, ShowStep, `Commu_LeaveWhenClosed` |
| `0x652DD0` | `CommuDraw_OpenSteps` | 2 | `0x939A40` | FadeOut, MusicIn (which sets state 1, step 0) |
| `0x652DD8` | `CommuDraw_ShowSteps` | 5 | `0x939A40` | Pick, Reveal, WaitKey, MusicBack, Close (state 2, step 0); byte data after |
| `0x652E48` | `CommuName_States` | 5 | `0x939A3E` | Begin, SlotStep, MemberStep, End, `Commu_LeaveWhenClosed` |
| `0x652E5C` | `CommuName_SlotSteps` | 9 | `0x939A40` | PanelReset .. SlotClose (cancel writes 8, SlotHow 6 / 7) |
| `0x652E80` | `CommuName_SlotRandomSteps` | 3 | `0x939A3F` | SlotRandomPick, Ask, Out |
| `0x652E8C` | `CommuName_SlotEntrySteps` | 5 | `0x939A3F` | SlotEntryIn .. SlotEntryDone |
| `0x652EA0` | `CommuName_MemberSteps` | 9 | `0x939A40` | PanelReset .. MemberClose |
| `0x652EC4` | `CommuName_MemberRandomSteps` | 3 | `0x939A3F` | MemberRandomPick, Ask, Out |
| `0x652ED0` | `CommuName_MemberEntrySteps` | 5 | `0x939A3F` | MemberEntryIn .. MemberEntryDone; byte data after |

`CommuDraw_OpenSteps` is two: the tool's "7 code entries" run on into
`CommuDraw_ShowSteps` (state 0 never passes step 1: `CommuDraw_MusicIn` sets
state 1 and step 0). Ours aborts with a message on an index past the count
(`Jump`, the round-nine rule); no state writes one.

`CommuName_RecordNames` `0x669FA4` (7): the pointers `CommuName_MemberEntryOut`
reads by R4E's `0x45F020` answer (a record 0..6, 0xFF none) - seven pointers
to five-byte names `0x669F70..0x669F9C`, text after them. Ours aborts past 7
(L4). **Not named here** (another group reads them too, or only): `0x652A70`
(R4B's `0x4572F0`, entries 7 and 8 ours), `0x6530C0` and `0x653150` (R4E's
`0x45F750` / `0x45FE00`, an entry each `Commu_LeaveWhenClosed`), `0x653200`
(20-byte slot rows, read by `CommuName_SlotEntryOut` and by code at
`0x455FB6`, R4A's band), the byte tables `0x652DEC` / `0x652E34` / `0x652E3C`
(CommuDraw's own, data not handlers: left unnamed, described in section 1).

## 4. The fuzz (`rest_4d_fuzz.cpp`)

One group, 60 clones, 4,000 rounds each, field mode; shapes: the dispatchers
and states `kState`, the board, the slot bar and frame, `RandBelow`,
`CountSlots` and `NthSlot` `kCall` with arguments set by `Args`;
`CommuDraw_RandBelow` and `CommuName_CountSlots` compared on al, `NthSlot` on
the whole eax. The ten state tables are `DataTable`s; each dispatcher's index
is seeded below its table's count (`kDispatch`).

**Callees.** The group's own called by `E8` / `E9` with what each reads; R4C's
`0x45B2C0`, `0x45B400` and R4E's eleven by address, each with the masks its
code reads (cited in the file: `movsx` words for coordinates, `and 0xFF` or
`mov al` for the bytes); `0x45ED70` fills the 0x20 bytes at `0x675F98` and
answers 0..0x24 (the caller copies that many); `0x45F020` answers a record
0..6 or 0xFF, never 0xFF under `CommuName_MemberEntryOut` (ours aborts there).
`Menu_DrawPanelBox` (as R2C lists it) and `Menu_DrawGreyHLine` (16-bit, 16,
16, a byte: `rest_2b.cpp`) listed; `BareRet` and `BareRetZero` with no
argument logged - they read none, so the PlayStation's arguments ours hands
`BareRet` are unobservable by construction (N38 below). Louder stand-ins (from the controls, section 6): `CommuName_NthSlot` answers a
slot 0..3 three times in four and moves a name byte of that slot one call in
four; `CommuDraw_RandBelow` answers 0, limit - 2 or limit - 1;
`CommuName_CountSlots` and R4E's `0x45F000` answer the cursor + 0..2 two times
in three; `CommuName_DrawHeader` moves the header word and `BareRet` the first
name byte one call in four. `Crt_sprintf` is
re-listed with five words, logging the numbers each of the group's formats
takes (three, one or none): the calls push two, three or five words, so the
standard row's third word is the caller's stack in two of them.

**The masks**: Capcom builds several coordinates in a 16-bit register whose
upper half is the previous call's eax (`movzx ax, byte [0x675F95]`, then
`lea` / `sub`); every callee reads those arguments' low word (R4E's
`0x45E870` passes them to `Menu_DrawBox` and `movsx` words, `0x45EC00` /
`0x45ECC0` / `0x45F1A0` `movsx`, `0x45F050` `and 0xFFFF`), so ours passes the
clean value and the row compares the word. The colours `push` a register whose
byte is the window colour (masked 0xFF).

**Regions** beyond field mode's: the community's cells `0x675F80..0x675FDF`,
`0x939A30..0x939A4F` (the record pointer `0x939A38`, the state bytes), the
game byte `0x9039F4`, the character records past the style region, the slot
cells and names `0x904700..0x904A8F`, the text scratch's rest and
`Text_Records` `0x904BC0..0x904DFF`, the `MessagePools` text `0x803980..0x803D7F`
the seeded offsets point into; 27,528 bytes in 45 regions.

**Seeds.** The buttons (a confirm bit, a cancel bit, the pressed word either,
both, none, or the repeat keys 0x8000 / 0x2000 / 0x1000 / 0x4000), `Input_Held`;
the state bytes small; the answers `0x939A3C` / `3D` 0 or not; the cursor at
0..7, 0xFF, 0xFE, 0x80; the answer at 0, 1, 2, 0xFF; the slide at 0..5, 0xC5,
0xC7, 0xC8; the header 0xF0 / 0xF1 / 0xFFFF / an id below 0x200; the entry's
answer and the first name byte 0 or not; title B 0xFF; the draw's picks small
(so a category's picks can all be equal: L1) and its counts below 10;
`Field_Request` 2 or not; the wait word 0 or not; the record pointer at a
scratch with `+5` a slot; the 60 slot cells half in use (one round in eight
all), the seven records' list bit half set, slot and record name bytes 0 now
and then; `MessagePools`' draw offsets and the header's first 0x100 pointing
into the seeded text, which has zeros and bit-7 bytes in it.

**Moves** (the group's case of the disturbance, `sh::DisturbCase(h, 15)`, from
the hash only): `0x939A40`, `0x939A3F`, `0x939A3E`, the cursor (0..7 while
`CommuDraw_Pick` runs: it indexes and writes by it), the answer, the slide, the
header, the entry's answer, the draw's counts (below 10), its picks (the first
name byte half the time), `Input_Pressed`, the column, the kept track, a slot
name byte, a record's flag or name byte. Each case has a control that misses
the re-read (section 6). **Not moved**: the entry's three bytes
`0x675FCA..CC` - read again after calls, but handed only to `BareRet`, which
reads none, so no control could see a stale one.

**In this worktree**: `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=rest_4d`, exit 0:
240,000 rounds over 60 functions, 1,734,968 calls to the stand-ins, **0
mismatches**; 317 stand-ins (174 field-standard). Every entry of the ten tables
was reached (the coverage line's `phase` rows), every listed callee called.

## 5. Latent defects and ranges (Capcom's, described, not fixed)

- **L1. The draw's "not taken" test is inverted in effect.** `CommuDraw_Pick`
  scans a category's earlier picks and keeps the new one at the first that
  differs from it; it moves on (`+ 1`, wrapping at the count) only when every
  earlier pick equals it. With two or more earlier picks the same name can be
  drawn twice in one category whenever one earlier pick differs. Reached by
  ordinary play whenever a row names a category three or more times (the rows
  at `0x652DEC` do). What the draw should do is the owner's; ours keeps the
  original's test (control D12 is the "meant" test, refused).
- **L2. `CommuDraw_RandBelow` never returns for a limit byte of 0** (`Rand &
  0x7F` is never below 0). Its three callers pass the counts of `0x652E34`,
  none 0; ours aborts before the first `Rand`.
- **L3. The PC's name entry is gone.** The PlayStation's entry steps' draws
  are calls of `BareRet` (a bare `ret`) with their arguments, and the answer
  `BareRetZero` is always 0. **Corrected 2026-10-05** (the coordinator, from
  [`name-entry-restoration.md`](name-entry-restoration.md)): the original does
  not end the entry in message 0xF7, it never leaves it - nothing in
  `CommuName_SlotEntry` / `_MemberEntry` writes the step byte `0x939A3F`, which
  the PlayStation's input step moved on, so `_SlotEntryOut` / `_MemberEntryOut`
  are never reached from them. **DIV-0075** (the owner's word, 2026-10-05):
  ours moves the step on after the answer, and the out step's unanswered
  branch (message 0xF7, state 4) ends the entry without a name; the switch is
  set after the self-test, which compares Capcom's steps. The entry itself
  waits for the localisation rework.

  **DIV-0075's row** (2026-10-05, round fourteen's end, debt 24; the form of
  DIV-0063's in `battle_e6_fuzz.cpp`). With the switch off in the fuzz, the
  moved step was built and seen armed but never run. `Rest4D_Inject` now
  calls `rest_4d::EntryAbandonTest()` once it has set the switch (under the
  same `BOF3X_SHADOW=rest_4d`, before the inject): ours'
  `CommuName_SlotEntry` and `CommuName_MemberEntry`, the switch on, each
  wrapped to take `0x939A3F` back by one after it returns, fuzzed against
  Capcom's two steps with the group's seed, moves, stand-ins and regions,
  4,000 rounds each. What the entry says ours must do is Capcom's step and
  the step one more, nothing else (no name changed, the answer still
  `BareRetZero`'s): a wrapper that matches means exactly that, and one that
  mismatches means ours does not move the step, moves it otherwise, or
  changes something more. Its lines in the shadow's log are the harness's
  `rest_4d DIV-0075 self-test: 8000 rounds over 2 functions ... 0 MISMATCHES`
  and `rest_4d DIV-0075: 2 entry steps, ours with the switch on, ...`. The
  next step's unanswered branch (message 0xF7, state 4) is Capcom's
  `_SlotEntryOut` / `_MemberEntryOut` unchanged, compared by the group's own
  rows. Controls on the row: the switch's step left out of
  `CommuName_SlotEntry`, and moved by two in `CommuName_MemberEntry`, each
  refused (section 6).
- **L4. Unbounded indexes**, one policy: a jump past a state table aborts
  (section 3); a dereference of `CommuName_RecordNames` past 7 aborts (R4E's
  `0x45F020` answers 0xFF when the cursor passes the records, and the original
  would read a dword of text as a pointer); byte reads past a table read the
  image in place as the original does (the slot names `0x9048F0 + 5 * 0xFF`
  for "no slot", the defaults `0x653200 + 20 * slot`, the draw's rows with a
  negative s8 cursor, the title's slot `+5`). No play value reaches one that
  the code shows: the cursors stay below the counts.
- **The message copy reads the byte before the message** (`p[-1]`) to decide
  whether a 0 ends it: for an empty message whose preceding pool byte has bit
  7 the copy runs past the 0 (to 0x20 bytes). `MessagePools` data, written by
  the loader; noted, not judged.
- **Nothing here needs a ledger entry**: no read of never-written memory
  reaches a draw or a decision. The registers pushed with stale upper halves
  (coordinates, the window colour, `BareRet`'s bytes) are each read by their
  callee as the word or byte ours passes, or not at all; `CommuDraw_Pick`'s
  two stack locals with stale upper bytes are masked to their bytes before use.
- **Not a full-frame fill**: `CommuDraw_Reveal`'s POLY_G4 and TILE are 320.0
  wide but start at y = the counter and y + 0x10 (not (0, 0) 320 x 240); left
  narrow, as `EffectKindA8_DrawBar` (DIV-0041's amendment) - for the
  coordinator.

## 6. Controls

`controls.py` in the session scratchpad (`.../r4d/`): each plant replaces
strings that occur once in `rest_4d.cpp`, rebuilds, runs `BOF3X_R4D_ONLY` on
the clone named, restores (the next plant's build is the restore's; one
rebuild of the restored file at the end); results in `controls.tsv`.

Every refusal is by a count of mismatching rounds (none by a crash or a Fatal
of ours). Rounds 4,000 a clone; counts from the final run (`controls_run5.out`).

| Ids | Function(s) | Plants | Refused | Weakest refusal |
|---|---|--:|--:|---|
| B01..B15 | the board | 15 | 15 | B12 the column read once (case 4), 7 |
| D01..D09 | CommuDraw's dispatchers, `FadeOut`, `MusicIn` | 9 | 9 | D08 the state read before the calls (case 2), 9 |
| D10..D23 | `CommuDraw_Pick` | 14 | 14 | D11 the count read before `RandBelow` (case 8), 10 |
| D24..D35 | `Reveal`, `WaitKey`, `MusicBack` | 12 | 12 | D33 the track read before the fade (case 12), 4 |
| D36..D40 | `Close`, `RandBelow` | 5 | 5 | D40 (509) |
| D41..D46 | `CommuDraw_DrawTitle` | 6 | 6 | D44 the cursor read before the first draw (case 3), 15 |
| N01..N03 | `CommuName_Dispatch`, `_Begin` | 3 | 3 | N02 (2,385) |
| N04..N35 | the slot states to the random name | 31 | 31 | N07 the step read before the sound (case 0), 7; N14, N30 7 |
| N36..N56 | the slot's entry, `SlotClose` | 21 | 19 | N47 the answer read once (case 7), 5 |
| N57..N69 | the member states to the confirm | 13 | 13 | N59 the flags read before the loop (case 14), 9 |
| N70..N95 | the member states from the slide out | 26 | 25 | N70 the cursor read once (case 3), 3; 618 with the panel's stand-in (2026-10-05, below), N84 then the weakest, 26 |
| N96..N99 | `CommuName_End`, `Commu_LeaveWhenClosed` | 4 | 4 | N97 (303) |
| N100..N113 | the counts, the bar, the header | 14 | 14 | N112 the header read before the box (case 6), 10 |
| | | **173** | **170** | |

(N15 was not planted: its anchor would have been N10's.)

**Every case of the disturbance has a control that misses its re-read**, each
refused: case 0 D05 (12), N07 (7); 1 N30 (7); 2 D08 (9); 3 B02 (319), D16
(68), D44 (15), N70 (3); 4 B12 (7); 5 D30 (18), N08 (13), N39 (13), N43 (36);
6 N19 (244), N112 (10); 7 N47 (5); 8 D11 (10); 9 N48 (38); 10 N61 (36); 11
N45 (25 of 24,000: the filter runs six clones); 12 D33 (4); 13 N41 (17); 14
N59 (9). The thin ones (3..7 of 4,000) are the round's usual: the group's
case runs after about one stand-in call in a few hundred.

**N70 made cheap (2026-10-05, round fourteen's end, debt 23).** R4E's `0x45EE10` (a member's panel) has a stand-in in the fuzz (`MemberPanelMove`) that one call in four moves the cursor to 0..7 after it, never while `CommuDraw_Pick` runs: `CommuName_MemberPanelOut` compares each record's place with the cursor again after every panel. N70 in this worktree: **618 of 4,000 rounds** (3 before). N57..N95, the controls of every function that calls the panel, re-run on that fuzz: all refused but N71, the equivalent (N59 9, N84 26 and N63 28 the weakest). The group's rounds stay 4,000, its shadow 0 mismatches.

**Not refused, equivalent:**
- **N36** (the entry's first `BareRet` at 0xB1) and **N38** (its last
  `BareRet`'s two bytes swapped): `BareRet` is a bare `ret` and reads no
  argument, so no input tells them apart. Near variant **N37** (the entry
  box, R4E's `0x45F1A0`, at 0x62) refused.
- **N71** (`CommuName_MemberPanelOut`'s moving row offset not cut to a byte):
  `14 * (n >> 1)` with n the records shown (at most 7) is below 0x100. Near
  variant **N72** (the moving one at 0x2C) refused.

**Strengthened by the controls** (section 4): N41 (a slot name byte kept across `NthSlot`) was 0 until `NthSlot`'s stand-in answered slots 0..3 and moved a name byte of the slot it answers; D13 (the wrap one early) was 1 of 4,000 until `RandBelow`'s stand-in answered 0, limit - 2 or limit - 1; N12, N19, N48, N64, N67 were 1 or 2 until the count stand-ins answered near the cursor, `CommuName_DrawHeader`'s moved the header and `BareRet`'s the first name byte; the whole batch was re-run on the final fuzz.

## 7. Calls across groups

**Outbound** (raw, `rest_4d_callees.h`, for the round's rebinding):

| Callee | Owner | From |
|---|---|---|
| `0x45B2C0` (a cell), `0x45B400` (a frame piece) | R4C | the board (11 sites) |
| `0x45E870` (a slot's panel) | R4E | the slot states (22 sites) |
| `0x45EC00` (a piece) | R4E | `CommuName_DrawSlotBar`, `_DrawSlotFrame` (9) |
| `0x45ECC0` (the slot hand) | R4E | `SlotChoose`, `SlotConfirm` |
| `0x45ED70` (a random name) | R4E | `SlotRandomPick`, `MemberRandomPick` |
| `0x45EE10`, `0x45EF90`, `0x45F000`, `0x45F020`, `0x45F050` (the members' panels, count, n-th, hand) | R4E | the member states |
| `0x45F1A0` (the entry's box) | R4E | the entry steps |
| `0x45F5A0`, `0x45F650` (the new name written; tail `jmp`s) | R4E | `MemberRandomOut`, `MemberEntryDone`; `SlotRandomOut`, `SlotEntryDone` |

`Menu_DrawPanelBox` (R2C) and `Menu_DrawGreyHLine` (R2B) are ours and called
by name. The edges table's R4D -> R2B 2, R2C 7, R4C 11, R4E 69 are these.

**Inbound from outside the group** (for the rebinding pass):

| Caller | Owner | Calls |
|---|---|---|
| `0x45B840`..`0x45C0C0` (R4C's states) | R4C | `CommuBoard_DrawRows` (15 sites), `_DrawRowCells` (9), `_DrawCells` (9) |
| `0x455450`, `0x455AA0` | R4A | `CommuName_CountSlots` |
| `0x45F650` | R4E | `CommuName_NthSlot` (4) |
| `0x4572F0` through `0x652A70[7]`, `[8]` | R4B | `CommuDraw_Dispatch`, `CommuName_Dispatch` |
| `0x45F750` through `0x6530C0[10]`, `0x45FE00` through `0x653150[1]` | R4E | `Commu_LeaveWhenClosed` |

**Harness rows**: no row of `scenario_harness*.cpp` or `boss_harness*.cpp`
names any of the 60 (`grep`).

## 8. The rebinding

`band_rows.py --refs --group R4D`: one raw reference to the 60 in `src/game` -
`area_w4d_callees.h:108`, a comment describing `0x455450`'s calls
("through 0x45E6B0 / 0x4560D0 / ..."), not a value: **left** (a comment of a
round-eleven group about another function's callees; nothing keys on it).
No `_callees.h` constant, fuzz key or table of originals refers to the 60.

## 9. The live route

None: no recorded route enters the community, and the catalogue's reach
columns hold no entry for any of the 60. **Fuzz only**; the owner records a
route once the code is ours.

## 10. Self-tests and the entry list

All in this worktree, headless (`BOF3X_SELFTEST_ONLY=1`), on the final build
(`e1898b35`'s code):

- `BOF3X_SHADOW=rest_4d`: exit 0, 240,000 rounds, 1,734,968 calls to the
  stand-ins, 0 mismatches.
- `BOF3X_SHADOW='*'`, narrow: ran to `self-test only: done`, 742 self-test
  lines, no mismatch, no Fatal; `rest_4d`'s line 240,000 rounds, 1,736,565
  calls (another stream than its own run), 0 mismatches.
- `BOF3X_SHADOW='*'` with `BOF3X_WIDE=1`: the same, 742 lines, no mismatch, no
  Fatal. No function of the group has an operand `widescreen.cpp` patches
  (nor `DIVERGENCE.md` or `cheats.cpp` a byte inside the 60). No run died
  silently.

`analysis/calltrace/entries_logic.txt` (the main checkout's): 49 lines
appended (2026-10-05), the 49 hidden starts with the extents read here. The
other eleven were listed already, nine with the extent read here; **two carry
a host's longer extent over this group's functions** and are left as they are,
for the coordinator to split: `0045C850 622` (the function is `0x61`; the line
runs over the ten hidden starts `0x45C8C0..0x45CE40`) and `0045CEA0 1810` (the
function is `0x19A`; the line runs over the 39 from `0x45D040`). Each hidden
start has its own line now, so the tracer's lookup by entry finds them.

`tools/ledger_check.py`: 73 ledger entries, 0 errors (2 notes, not this group's).
