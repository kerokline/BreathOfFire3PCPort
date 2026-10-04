# Group R2C: the inn's and save point's last states, the save block, the shop's browse and sell modes, the master's talk

**Status:** MEASURED (2026-10-04) - round fourteen
([`takeover-queue-round14.md`](takeover-queue-round14.md)), wave two, from the
round branch's tip `fa583bc`. **61 functions ours** (`src/game/rest_2c.cpp`,
declarations in `src/game/rest_2c.h`, the tables' and cells' addresses in
`src/game/rest_2c_callees.h`, shadow name `rest_2c`): the cut's 60 rows for R2C
(`analysis/round14_cut.tsv`) and `ShopMode_States[9]` `0x583350`, a start no
list had (section 2); each read to its last instruction with capstone and
fuzzed through the scenario harness in field mode
([`scenario_harness.md`](scenario_harness.md) section 7), used unchanged:
244,000 rounds, 0 mismatches. **188 controls planted** one at a time: 186 refused by a count, 2 equivalent mutants not refused, each with its near variant refused (section 6). No recorded route is traced
entering any of the 61 (section 9): fuzz only here.

The band is not one thing. In address order it holds: **the figure record's
moves** (`0x9398E0`, the record R2B's `0x57F320` dispatches on its `+1`
through `0x663E28`); **the inn's, save point's and rest's last states** (the
other states are `shop_states.cpp`'s, group DF of round eight, and
`field_s.cpp`'s, group FS of round twelve); **the save block's builder** and
`Save_QuickWrite` (F12); **four dispatchers of `ShopMode_States`** (5, 6, 7, 8,
9 and 10 - their states are DF's, DG's, FS's and this group's); **ShopMode 8**
(`ShopSell_States`' own four) and **ShopMode 9** (the "browse" machine
`field_s.md` 1.3 called nobody's); **the master's talk** (`0x9398CF` mode,
`0x9398D1` step - the machine `FieldTail_LoadBank` runs) and **the panels** it
and R2D's master screens draw. What these screens are in play is not read
here; the names say what the code does, and "master" follows
[`yes-no-prompts.md`](yes-no-prompts.md) section 2 (`0x586D20`, R2D's, is "the
master's prompt", indexed by the same byte `0x9039F5` and table `0x6644E8`).

PSX twins (`analysis/pairs_propagated.json`, hypotheses, not read):
`0x585BE0` 0x801D144C, `0x585DC0` 0x801D9810 / 0x801D1784 / 0x801E2F78,
`0x586160` 0x801D1E5C. The sibling's `names/*.toml` and `symbols.toml` name
none of them, so every name here is from the PC code.

## 1. What each function does

The menu block (`0x929F00`): mode `+0` (`ShopMode_States`' index), state `+1`,
step `+2`, timer `+4`, answer `+0xB`, the touched object `+0xC` (0xFF none),
the object kept `+0xF`. "The title box" is
`Menu_DrawTitleBox(0x14, 0x12, 0x118, 0x13, the window colour 0x903A5A)`;
"system message n" is `Text_DrawAt(0x1C, 0x16, 0, 0xFF, Msg_SystemPtr(n))`.

### 1.1 The figure record (`MasterFigure_States` `0x663E28`, by `0x9398E1`)

| Function | Entry | Bytes | What |
|---|---|--:|---|
| `MasterFigure_DrawFaded` | `0x57F340` | 0xD5 | the record drawn by R2B's `0x57EEF0` with its colour bytes `+0x5D..+0x5F` faded by 3 x the step `+0x107` (first and third down to 0, second up to 0xFF, byte arithmetic) and put back after the draw; the scale `+0x40` = `0x663D7C[+0x10B]` |
| `MasterFigure_TurnHome` | `0x57F420` | 0x2E | entry 2: the angle `+0x6C` 0x40 toward 0 (`& 0xFC0`), `+6` = 1 when home; the faded draw (a tail jump) |
| `MasterFigure_Settle` | `0x57F450` | 0x87 | entry 3: the height `+0x3C` falls by a sixteenth of the gap to `(AreaMap_Elevation(Field_Kind2X, Field_Kind2Z) + 0x200) << 16`, kept at or above the ground plus `+0xC0 * 0xA00` (`+6` = 1 there); the faded draw |
| `MasterFigure_Hold` | `0x57F4E0` | 0x5 | entry 4: the faded draw alone |
| `MasterFigure_TurnOn` | `0x57F4F0` | 0xC | entry 5: the angle + 0x20 (entry 1, R2B's `0x57F330`, takes 0x20 off); the faded draw |

### 1.2 The inn, the save point, the rest

| Function | Entry | Bytes | What |
|---|---|--:|---|
| `InnPrompt_NotEnough` | `0x57FA40` | 0x6F | `InnPrompt_States[4]` (the choice when the zenny is short): the choices at (0x6E, 0x4C), the title box, the zenny box with an object, system message 0xA2; a button back to the choice |
| `FieldSave_Write` | `0x580010` | 0xB1 | `FieldSave_States[4]`: system message 0x99, the slots unlit; `Save_BuildBlock`; the file's name from the slot `0x9036D4` (`Crt_sprintf` with `0x664068`) and `Save_WriteFile(name, 0x12B0)`; unless -1 (DIV-0003's path, a bare return): the sub-state up, timer 0x1E, the slot's summary, `0x6BC881` = 2 |
| `FieldSave_Written` | `0x5800D0` | 0x7F | `FieldSave_States[5]`: system message 0x8F, the slots; `0x6BC880` = 1; the timer or a button, then sound 0x104 and the sub-state up |
| `FieldSave_PromptAnswer` | `0x580230` | 0x4D | `FieldSave_States[8]` (`FieldSave_End`'s question, message 0xD0): once the message is done, answer 0 - the way out (step up); else back to the save menu (step 3) |
| `Inn_TitleOut` | `0x580280` | 0x77 | `Inn_Steps[4]`, the way out: the title box rising 20 a frame for five frames, then mode 1 and, under `Inn_Begin`'s test (Field_InputFlags 0x40, areas 0xBC, 0x85, 0xC1), the object back from `0x929F0F` |
| `Rest_Dispatch` | `0x580300` | 0xE | `ShopMode_States[10]`: `Rest_States` (7) by the state |
| `Rest_LoadJingle` | `0x580560` | 0x23 | `Rest_States[2]`: black, `Sound_LoadStream(0)`, timer 0x96 |
| `Rest_WaitJingle` | `0x580590` | 0x39 | `[3]`: black; the timer down, then on `Sound_StreamDone` the state up and `Transition_Start(1)` |
| `Rest_Restore` | `0x5805D0` | 0x21 | `[4]`: with the wait word 0, `Party_RestoreAll(0)` |
| `Rest_End` | `0x580600` | 0xF | `[5]`: mode 1, state 0 |
| `Rest_EndAfterMessage` | `0x580610` | 0x18 | `[6]` (`Rest_Begin`'s with its message 0xEF): once the message is done, mode 1 |
| `Save_BuildBlock` | `0x5806F0` | 0x1E9 | the live block `0x9039E0` gathered (chapter dwords, counters, ObjTrio record 0's position and `+8`, the area, `0x903A50` = 0), the summary `0x904680` built (the buttons' words, the leader's name by `strncpy` 5 + 4 bytes, the three party ids - id 4 is 0xB unless `0x90412C & 0x7F` is 5 or 0xC -, record 0's `+0xA` / `+0xC`, `0x9040C8`, the style bytes, `Flags_Test(0x904030, 0x92)`), `0x90412D` = `0x9039A2`; the block's 0x10B0 bytes copied to `0x92A0E0` and summed (low word at `0x92A150`), 0xD50 more cleared, the summary copied to the slot's |
| `Save_QuickWrite` | `0x5809C0` | 0x72 | F12 (`Game_WndProc`): slot 0, the name, the block, the name again, the file; unless -1 the summary |

### 1.3 The shop's modes

| Function | Entry | Bytes | What |
|---|---|--:|---|
| `ShopResist_Dispatch` | `0x5837E0` | 0xE | `ShopMode_States[5]`: `ShopResist_States` (8) by the state |
| `PartyForm_Dispatch` | `0x580A40` | 0xE | `ShopMode_States[6]`: `PartyForm_States` (4) |
| `SharedList_Dispatch` | `0x584180` | 0xE | `ShopMode_States[7]`: `SharedList_States` (5) |
| `ShopSell_Dispatch` | `0x582EB0` | 0xE | `ShopMode_States[8]`: `ShopSell_States` (4) |
| `ShopSell_Open` | `0x582EC0` | 0xED | state 0: windows 0, 1 and 7 placed and on, the hand (21) set up off, `0x6BC8AA` = 1, `0x6BC8A4` = 0; state + 2 (to the sell steps) |
| `ShopSell_Leave` | `0x582FB0` | 0x33 | state 1: windows 0, 1, 7 out, the hand off, timer 5; state + 2 |
| `ShopSell_SellStep` | `0x582FF0` | 0xE | state 2: `ShopSell_SellSteps` (5) by the step (the trade screen's sell steps, `ShopTrade_SellSteps`' five) |
| `ShopSell_LeaveWait` | `0x583000` | 0x16 | state 3: the timer down, then mode 1 |
| `ShopBrowse_Dispatch` | `0x583350` | 0xE | `ShopMode_States[9]`: `ShopBrowse_States` (3) by the state |
| `ShopBrowse_OpenStep` | `0x583360` | 0xE | state 0: `ShopBrowse_OpenSteps` (2) by the step |
| `ShopBrowse_Open` | `0x583370` | 0x26 | `ShopBrowse_InitWindows`, timer 6, sound 0x102 |
| `ShopBrowse_OpenWait` | `0x5833A0` | 0x22 | the timer down, then state 1 |
| `ShopBrowse_ChooseStep` | `0x5833D0` | 0xE | state 1: `ShopBrowse_ChooseSteps` (3) |
| `ShopBrowse_Choose` | `0x5833E0` | 0x204 | a list of 17 (cursor `+0xB` of window 1, 0..0x10), nine shown from `+0xA`; up / down by one with a scroll (`+8` 0xF0 / 0x10) at the edges, `0x0004` / `0x0008` a page of nine; the hand placed; confirm on an entry (`+0xD` not 0xFF) opens the detail (`ShopBrowse_OpenDetail`), on none 0x107; cancel to state 2 |
| `ShopBrowse_Detail` | `0x5835F0` | 0x85 | the help line (window 0 `+0x10` = the entry + 0x122), a timer, then cancel closes the detail |
| `ShopBrowse_DetailClose` | `0x583680` | 0x16 | the timer down, then back two steps (the list) |
| `ShopBrowse_CloseStep` | `0x5836A0` | 0xE | state 2: `ShopBrowse_CloseSteps` (3; entry 0 is FS's `Menu_StepAfterTimer`) |
| `ShopBrowse_CloseNext` | `0x5836B0` | 0x7 | the step up |
| `ShopBrowse_End` | `0x5836C0` | 0x14 | mode 1, state and step 0 |

### 1.4 The master's talk (`MasterTalk_States` `0x66450C`, by `0x9398CF`)

`0x9039F5` is the master (the index of the per-master u16 table `0x6644E8`,
each the base of that master's script messages); a character record's
`+0x1F` is its master (0xFF none: `MasterPanel_DrawStats` shows system
message `0x111 +` it).

| Function | Entry | Bytes | What |
|---|---|--:|---|
| `MasterTalk_Reset` | `0x585A00` | 0x1C | `FieldTail_LoadBank`'s step 1: the mode, step, `0x9398D0`, `0x9398CD` and slide 0 |
| `MasterTalk_Dispatch` | `0x586670` | 0xE | `FieldTail_LoadBank`'s step 2 (a tail jump): `MasterTalk_States` (7) by the mode |
| `MasterTalk_Begin` | `0x586680` | 0x49 | mode 0: introduced (`Flags_Test(0x904657, master)`) - mode 5; master 9 - script message 0x2E and mode 6; else mode 1 |
| `MasterTalk_IntroStep` | `0x5866D0` | 0xE | mode 1: `MasterTalk_IntroSteps` (2) by the step |
| `MasterTalk_IntroSay` | `0x5866E0` | 0x3D | `Flags_Set(0x904654, master)`, the master's base message |
| `MasterTalk_IntroWait` | `0x586720` | 0x2B | once the message closed, `Flags_Set(0x904657, master)`, mode 3 |
| `MasterTalk_AskStep` | `0x586750` | 0xE | mode 2: `MasterTalk_AskSteps` (6) |
| `MasterTalk_Say5`, `_Say6`, `_Say8` | `0x586760`, `0x586790`, `0x586860` | 0x30, 0x39, 0x39 | the base + 5, 6, 8 (6 and 8 once the message closed) |
| `MasterTalk_CheckAllPupils` | `0x5867D0` | 0x88 | a member whose master is not this one: mode 3; else base + 7 |
| `MasterTalk_CheckAnyPupil` | `0x5868A0` | 0x88 | a member whose master is this one: mode 4; else base + 9 |
| `MasterTalk_SayFarewell` | `0x586930` | 0x3B | base + 4, mode 6 |
| `MasterTalk_PickStep` | `0x586970` | 0xE | mode 3: `MasterTalk_PickSteps` (9; four entries R2D's) |
| `MasterTalk_PanelsOpen` | `0x585A20` | 0x21 | sound 0x102, slide 4 |
| `MasterTalk_PanelsIn` | `0x585A50` | 0xC9 | the slide down; per member `MasterPanel_DrawMember` and `MasterPanel_DrawStats`, then the title panel `Menu_DrawPanelBox(0x14, 0x10 - 10 * slide, 0x118, 0x13)`; at 0 the pick `0x9398CE` = 0 |
| `MasterTalk_PanelsOut` | `0x585B20` | 0xBC | the slide up, the same; at 4 the step up |
| `MasterTalk_PickAsk` | `0x586980` | 0x1F | R2D's `0x5869A0(base + 0xF, 0)` |

### 1.5 The panels

| Function | Entry | Bytes | What |
|---|---|--:|---|
| `MasterPanel_DrawStats` | `0x585BE0` | 0x1D8 | (x, y, record): the four stats with DIV-0064's labels `0x66A0F8..`, the record's master line, the box's pieces |
| `MasterPanel_DrawMember` | `0x585DC0` | 0x26F | (x, y, record, slot): the face, name, level, status words, HP and AP coloured, the experience bar, the slot's number cell (R2H's `0x59DB70`) |
| `MasterPanel_DrawExpBar` | `0x586030` | 0xD4 | (x, y, record, level, exp): a 57-unit LINE_F2, colour (0x80, 0, 0), the level's progress |
| `MasterPanel_ExpForLevel` | `0x586110` | 0x45 | (record, level): `Char_ExpForLevel`'s body to the instruction (a second copy) |
| `Menu_DrawPanelBox` | `0x586160` | 0x404 | (x, y, w, h, colour): `Menu_DrawTitleBox`'s body to the instruction (a second copy; ours in `menu_windows.cpp`) - four FT4 edges from the 16 x 16 cell at (0, 0xF0) and an outline |
| `MasterPanel_DrawFace` | `0x586570` | 0xF9 | (x, y, id, shade): a 40 x 48 face from `0x6644B8`, shaded 0 / 1 / else as `0x573E50`'s icons |

## 2. The extents, the starts

`band_rows.py --group R2C --byte-tables` (and the scratch wrapper's same run):
60 functions, the tool's extents equal to the code's for all 60, the cut's
sizes padding past them (48 rows). Every row was read again to its last
instruction (the ends are in `symbols.toml`'s evidence).

- **Added: `0x583350`** (`ShopMode_States[9]`, 0xE bytes: `jmp
  [0x664190 + 4 * 0x929F01]`). The cut lists its three states' dispatchers
  (`0x583360`, `0x5833D0`, `0x5836A0`) but not it; a scan of the band's bytes
  against every owned extent (`entries_logic.txt`) and the cut's (scratch
  `gaps.py`) found it the only code of the band in no list and no extent
  (`Shop_Equip`'s 0x138 ends at `0x583348`, its jump table included).
- **Dropped: none.** No start is a jump-table case, a shared tail or data.
  The tail jumps into `MasterFigure_DrawFaded` (`0x57F420`, `0x57F4E0`,
  `0x57F4F0`, and R2B's `0x57F330`) reach a function with its own frame and
  `ret`: it is a function, and theirs are calls to it.
- **Two copies of code already ours**: `MasterPanel_ExpForLevel` is
  `Char_ExpForLevel` (`0x574A60`, `menu_windows.cpp`) instruction for
  instruction, `Menu_DrawPanelBox` is `Menu_DrawTitleBox` (`0x574AB0`); each is
  reached by its own callers (`E8` to its own address), so each is a function
  of its own, written out again here (de-duplication is the refactor the round
  leaves for after it).
- **Hidden starts in hosts already ours** (the cut's `host` column):
  `0x582EB0..0x583000` (in `PartyForm_Draw`'s catalogue extent),
  `0x583360..0x5836C0` (in `Shop_Equip`'s), `0x5837E0` (`ShopBrowse_OpenDetail`'s),
  `0x584180` (`ShopResist_Message`'s). None of those hosts' code in
  `field_s.cpp` / `save_menu.cpp` contains any of them (each host ends at its
  own `ret` before them); each is reached by a `.data` table, so each is taken
  as its own function.

## 3. The state tables

Nine named here (`[[data]]`, count by the reader's reach read by hand: each
dispatcher is unbounded, and its table runs to the next table's first cell,
whose own reader starts there):

| Table | At | Count | Reader (index) |
|---|---|--:|---|
| `MasterFigure_States` | `0x663E28` | 6 | R2B's `0x57F320` (`0x9398E1`); `ShopMode_States` follows |
| `ShopBrowse_States` | `0x664190` | 3 | `ShopBrowse_Dispatch` (state) |
| `ShopBrowse_OpenSteps` | `0x66419C` | 2 | `ShopBrowse_OpenStep` (step) |
| `ShopBrowse_ChooseSteps` | `0x6641A4` | 3 | `ShopBrowse_ChooseStep` |
| `ShopBrowse_CloseSteps` | `0x6641B0` | 3 | `ShopBrowse_CloseStep`; `ShopResist_States` follows |
| `MasterTalk_States` | `0x66450C` | 7 | `MasterTalk_Dispatch` (`0x9398CF`; the modes written are 0..6) |
| `MasterTalk_IntroSteps` | `0x664528` | 2 | `MasterTalk_IntroStep` (`0x9398D1`) |
| `MasterTalk_AskSteps` | `0x664530` | 6 | `MasterTalk_AskStep` |
| `MasterTalk_PickSteps` | `0x664548` | 9 | `MasterTalk_PickStep`; R2D's `0x587120` reads `0x66456C` |

Already named and read by this group's dispatchers: `Rest_States` (7),
`PartyForm_States` (4), `ShopSell_States` (4), `ShopSell_SellSteps` (5),
`ShopResist_States` (8), `SharedList_States` (5). The tables this group's
states sit in and others read (`ShopMode_States`, `Inn_Steps`,
`InnPrompt_States`, `FieldSave_States`) are named already. **Not named, for
R2D**: `0x66456C` (R2D's `0x587120` reads it; it holds three of this group's
functions, `MasterTalk_PanelsOpen`, `_PanelsIn`, `_PanelsOut`).
`MasterFigure_States` is named here although its reader is R2B's: four of its
six entries are this group's and the brief's hint gave it to R2C - **if R2B
names `0x663E28` too, the coordinator keeps one**.

## 4. The fuzz (`rest_2c_fuzz.cpp`)

One group, 61 clones, 4,000 rounds each, field mode, shapes: the menu block's
states and dispatchers `kMenu`, the master's and the figure's `kState`, the
draw helpers and the block builder `kCall` with their arguments set by `Args`,
`MasterPanel_ExpForLevel` compared on the whole `eax`. The fourteen tables the
dispatchers read are `DataTable`s; each dispatcher's index is seeded below its
own table's count (`kDispatch`).

**Callees.** Every one the standard set types otherwise is listed with what it
reads: the title box, money box, box, pieces, outline, text and font calls
(masked as FS's, `field_s_fuzz.cpp`), `Msg_SystemPtr` answering into the text
buffer, `Crt_sprintf` writing letters, `Save_WriteFile` answering -1 a third of
the time, `Input_AutoRepeat` answering each key the browse tests,
`AreaMap_Elevation` answering heights the seed puts the figure around,
`MasterPanel_ExpForLevel` answering totals that agree a third of the time;
the group's own functions called by `E8` / `E9` by their addresses and our
keys; R2B's `0x57EEF0`, R2D's `0x5869A0` (`{0xFFFF, 0xFF}`: `and ecx, 0xFFFF`
and `dl`) and R2H's `0x59DB70` (`{0xFFFF, 0xFFFF, 0xFF, 0xFF, 0xFFFF, 0xFF}`)
by address; the C runtime's `strncpy` `0x5B9450` through (`kThrough`: both
sides copy for real). Masks where Capcom pushes a whole register for a byte:
the window colour (an entry register's upper bytes), `MasterPanel_DrawStats`'
record (a callee's answer above `al`), the member panel's x (an answer's
upper half times 40 - the callees read 16 bits), the panel box's colour,
`Msg_SystemPtr`'s id (`ecx`'s upper half; ours masks it to 16, `msg_pool.cpp`).
`Menu_DrawPanelBox` is listed with 16-bit x, y, w, h, as `Menu_DrawTitleBox`:
`MasterTalk_PanelsIn` with no member pushes `y` from a register whose upper
half is the entry's (`mov al, count` left it), 0 in the game (the dispatcher
zero-extends) and garbage under the fuzz; every reader of it uses 16 bits.

**Strengthened by the controls** (section 6): R2B's draw stand-in logs the
figure record's first 0x110 bytes at the call (the faded colour is put back
after it, so only the draw sees it); `AreaMap_Elevation` answers one height a
round, chosen by the seed, which puts the height exactly where the fall lands
on the target and one either side; the timer's seed has 4 (the title's last
slide frame) and the areas their neighbours; the eight records' HP at 0, 1, 2,
AP at 0 and around a quarter of its maximum, the status bits; a record's
level and experience move after `Menu_DrawPieces` and the panels' own calls.

**Regions** beyond field mode's: `WindowRecords` (22), `0x6BC880..0x6BC8C8`,
`0x903580..0x90358B`, the save block `0x9039E0..0x903A03`, the records past the
style region `0x903A94..0x903F90`, the save block `0x904160..0x904560` and
`0x904700..0x904A90`, the staging copy and its cleared tail
`0x92A0E0..0x92BEE0`, the sixteen summaries, the slot, the master's bytes and
the figure record `0x939880..0x939A00`, the message flags `0x7DEE40..`; 36,244
bytes in 50 regions.

**Seeds.** The buttons (a confirm bit, a cancel bit, the pressed word hitting
either, both or none); the timer at 0, 1, 2, 5, 6, 0x1E; the answer, the
object (0xFF, 0xFE), the message flag, Field_InputFlags 0x40, the areas
0xBC / 0x85 / 0xC1 and neighbours, the wait word, the slot below 16; window
1's first shown (0, 1, 7..10) and cursor at the paging edges, the scroll, the
entry (0xFF); the master (9 often, 0..17), Field_Request 2 or not, the slide
0..5, the member count 0..3, their record indexes 0..7 and each record's
master equal to this one (all, some, none); the figure's fade step and colour
bytes at 3 x step and 0xFF - 3 x step, the scale index (21 entries mostly),
the angle's low bits 0 or not, the height around the target; party id 4 and
`0x90412C` at 5 / 0xC / others. **Moves** (the group's disturbance and the
stand-ins' effects, from the hash only): the state, step, timer, slot,
window 1's cursor / first / entry, the pressed word, the master's step and
slide, the member count and records, Field_Request, the figure's colour and
height, the master byte.

**In this worktree**: `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=rest_2c`, exit 0:
244,000 rounds over 61 functions, 621,662 calls to the stand-ins, **0
mismatches**; 330 stand-ins (174 field-standard). Every entry of the fourteen
tables was reached (the coverage line lists each `phase`), every listed callee
called.

## 5. Latent defects and ranges (Capcom's, described, not fixed)

- **The dispatchers are unbounded** (all fourteen, and R2B's `0x57F320`): an
  index past a table jumps through the next table's cells. No state writes
  one; ours aborts with a message.
- **The save slot is unbounded**: `FieldSave_Write`, `Save_BuildBlock` and
  `Save_QuickWrite` write the summary at `0x905BC0 + 0x1C * [0x9036D4]` with
  the whole dword. Every writer keeps it 0..15; ours aborts past it.
- **Reads past a table, in place** (ours reads the same bytes): the figure's
  scale index `+0x10B` (21 scales; past them the state tables), the master
  byte into `0x6644E8` (18 masters), a face id into `0x6644B8` (12 faces), a
  record byte into the character records (8) and `Char_ExpTable`, a party id
  into `MoveScript_EffectState` (24). No play value goes past them that the
  code shows.
- **`Save_BuildBlock` takes the name from the leader's record but the level
  and `+0xC` from record 0** (`0x903A7A`, `0x903A7C`), whatever the party;
  described, not judged: what the summary should show is the owner's.
- **`Menu_DrawPanelBox` writes into its own argument slots** (h - 2's byte,
  then x + w - 1 and its float over the colour's): the caller's frame, which
  no caller reads after the call.
- **Nothing here needs a ledger entry**: no original read of never-written
  memory reaches a draw or a decision. The registers pushed with stale upper
  bytes (the window colour, the panel's y with no member, `MasterTalk_PickAsk`'s
  message in `ecx`, the font colour over the y slot) are each read by their
  callee as the byte or word ours passes; `MasterPanel_DrawExpBar`'s
  zero-length branch stores the float x it wrote to a local, not an
  uninitialised one.

## 6. Controls

`controls.py` in the session scratchpad (`.../r2c/`): each plant replaces
strings that occur once in `rest_2c.cpp`, rebuilds, runs `BOF3X_R2C_ONLY` on the
clone(s) named, restores and rebuilds; results in `controls*.tsv`. Every
refusal is by a count of mismatching rounds (none by a crash or a Fatal of
ours).

| Ids | Function(s) | Plants | Refused | Weakest refusal |
|---|---|--:|--:|---|
| F01..F15 | the figure record | 15 | 14 | F10 at the target (1,172 of 4,000) |
| I01..I04 | `InnPrompt_NotEnough` | 4 | 4 | I01 (3,024) |
| W01..W09 | `FieldSave_Write`, `_Written` | 9 | 9 | W06 the slot read before the write (181) |
| P01..P03 | `FieldSave_PromptAnswer` | 3 | 3 | P01 (976) |
| T01..T05 | `Inn_TitleOut` | 5 | 5 | T03 area 0x86 (87) |
| R01..R10 | the rest | 10 | 10 | R10 the state up after the transition (6) |
| B01..B12 | `Save_BuildBlock` | 12 | 12 | B08 `0x9039A2` read before `Flags_Test` (91) |
| Q01..Q03 | `Save_QuickWrite` | 3 | 3 | Q02 (1,337) |
| D01..D14 | the fourteen dispatchers | 14 | 14 | D03 (3,182) |
| S01..S08 | ShopMode 8 | 8 | 8 | S07 (1,011) |
| O01..O04, C01..C16, E01..E07 | ShopMode 9 | 27 | 27 | C06 the scroll at first + 8 (70) |
| M01..M28 | the master's talk | 28 | 28 | M28 the master read before `Flags_Set` (9), M07 (16) |
| K01..K20 | the stats and member panels | 20 | 20 | K20 the level read before the pieces (32) |
| X01..X07, L01..L03 | the bar, the totals | 10 | 10 | L01 (465) |
| Y01..Y13 | `Menu_DrawPanelBox` | 13 | 12 | Y13 (805) |
| Z01..Z07 | `MasterPanel_DrawFace` | 7 | 7 | Z06 id 4 as 0xA (320) |
| | | **188** | **186** | |

**Not refused, equivalent:**
- **F02** (`MasterFigure_DrawFaded`, the second colour byte's test `>` made
  `>=`): at equality, `0xFF - G == 3 * step`, the original's 0xFF and the
  mutant's `G + 3 * step` are the same byte - no input tells them apart. Near
  variant **F15** (the test against `t + 1`) refused, 370 rounds.
- **Y07** (`Menu_DrawPanelBox`, the half an arithmetic shift made logical):
  the half is used only as its short and its low byte, which both shifts give
  alike for every `w + 1` below 4. Near variant **Y13** (`/ 2`, rounding toward
  0) refused, 805 rounds.

**Refused only after the fuzz was strengthened** (the first run did not; the
fixes are in section 4): F01, F10, T03, K12, K20, M27 (the stand-ins for the
panels' calls moved nothing they re-read), and T02, T04, T05 (refused in 1 to
5 rounds before the timer seed had 4). Three anchors (M20, M21, M24) matched a
comment as well and were re-anchored. The first batch was not re-run on the
stronger fuzz: it only adds seeds and stand-in effects.

## 7. Calls across groups

**Outbound** (raw, `rest_2c_callees.h`, for the round's rebinding):

| Callee | Owner | From |
|---|---|---|
| `0x57EEF0` (the figure's draw) | R2B | `MasterFigure_DrawFaded` |
| `0x5869A0` (the master's pick) | R2D | `MasterTalk_PickAsk` |
| `0x59DB70` (an 8 x 8 cell) | R2H | `MasterPanel_DrawMember` |
| `0x5B9450` (`strncpy`) | the C runtime | `Save_BuildBlock` (`field_e2_callees.h` calls it `kMemcpy`; it stops at a NUL) |

**Inbound from outside the group** (for the rebinding pass):

| Caller | Owner | Calls | How |
|---|---|---|---|
| `0x57F330` (`MasterFigure_States[1]`) | R2B | `MasterFigure_DrawFaded` | tail `E9` |
| `0x5869A0`, `0x586D20` | R2D | `MasterPanel_DrawMember`, `_DrawStats`, `Menu_DrawPanelBox` | `E8` (6 sites) |
| `0x45D0D0`, `0x45D3E0`, `0x45D9E0`, `0x45DAB0`, `0x45DE60`, `0x45E570`, `0x45E820` | R4D | `Menu_DrawPanelBox` | `E8` |
| `EffectKind55_DrawTime`, `EffectKind55_DrawCount`, `EffectKind77_DrawCount` | ours (E2G, E3D) | `Menu_DrawPanelBox` | `SH_AT(at::kBox)`, rebound |
| `FieldTail_LoadBank` | ours (FE2) | `MasterTalk_Reset`, `MasterTalk_Dispatch` | `SH_AT(at::kLoadWait / kLoadStep)`, rebound |
| `Game_WndProc` (F12) | ours | `Save_QuickWrite` | by name |
| the shop's, inn's and save point's tables | `.data` | the states | in place |

**Harness rows that list this group's functions by address**
(`scenario_harness.cpp`, not this group's to edit): the field-standard rows
`0x586670` (no arguments) and `0x585A00` (no arguments, no calls) - both match
the reading; the effect-standard `FX_RAW(0x586160)` (five words `kAll`) -
right in arity, and the reading says the callee uses x, y, w, h as 16-bit and
the colour as a byte (stricter, not wrong: for the fold). They key on the
address, and every caller of ours calls by the address (the constants keep
their values), so none breaks. `scenario_harness_fh.cpp` clones `0x5837E0`
(`ShopResist_Dispatch`) as "FS 0x5837E0" against the function in place, which
is now ours: it still passes under `'*'` (section 10).

## 8. The rebinding

`band_rows.py --refs --group R2C` (34 raw references to 13 of the 60) and
`grep -rn -i` of the 61 over `src`:

| File | Change |
|---|---|
| `effect_2g_callees.h` | `kBox = bof3::addr::Menu_DrawPanelBox` (value unchanged; the header now includes `symbols.gen.h`) |
| `effect_3d_callees.h` | the same |
| `field_e2_callees.h` | `kLoadWait = bof3::addr::MasterTalk_Reset`, `kLoadStep = bof3::addr::MasterTalk_Dispatch` |

**Left raw, on purpose**: the `CallSite` tables of `effect_2g_fuzz.cpp`,
`effect_3d_fuzz.cpp`, `field_e2_fuzz.cpp` (the disassembly's targets);
`scenario_harness.cpp`'s three rows and `scenario_harness_fh.cpp`'s clone (a
harness); `shop_states_callees.h`'s `kTablesEnd = 0x663FB8` (`Rest_States`,
FS's table, not a function); comments in `shop_states.cpp`, `shop_states2.cpp`,
`field_s.cpp`, `field_e2.cpp`, `effect_2g.cpp`, `effect_3d.cpp` describing the
state before this round. No file of another group of this round refers to the
61.

## 9. The live route

The catalogue's reach columns hold no entry for any of the 61 (the eleven
hidden in `Shop_Equip`'s extent carry the shop column's -1, the host's, not a
+); no trace under `analysis/calltrace` names one; `HANDOFF.md`'s camp route
(`campingFishing.txt`) enters none (its camp functions are R2E's to R2H's).
Fuzz only here. **Likely, not traced**: `shop.txt` walks the inn (its frames
2520 and 2880 are the inn's menu and the stacked "Save?",
[`yes-no-prompts.md`](yes-no-prompts.md) section 6) - `Inn_TitleOut`, the
`FieldSave_*` states and, if it saves, `Save_BuildBlock`; the owner's
`masterAndManillo.txt` talks to a master (`yes-no-prompts.md` section 7: the
"Is this OK?" at frames 3990..4980 is R2D's `0x586D20`, which draws this
group's panels) - the `MasterTalk_*` states and the panels. Both are the
coordinator's A/Bs after the merge (the state hash).

## 10. Self-tests and the entry list

SELFTESTS

`analysis/calltrace/entries_logic.txt` (the main checkout's): 55 lines
appended (2026-10-04) - the 51 starts it lacked and the smaller extents
`0057F340 D5`, `005809C0 72`, `00585A00 1C`, `00586570 F9` beside the
catalogue's host lines (`1C0`, `90`, `1DC`, `42F`), which carry a host's
longer extent over this group's functions: **for the coordinator to split**.
The other six were listed with the extents read here.

`tools/ledger_check.py`: LEDGER
