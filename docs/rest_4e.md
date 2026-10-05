# Group R4E: the community band's panels, name commits, track list, item screen and ranked lists

**Status:** MEASURED (2026-10-05) - round fourteen
([`takeover-queue-round14.md`](takeover-queue-round14.md)), wave four, from the
round branch's tip `35ec19ef`. **48 functions ours** (`src/game/rest_4e.cpp`,
`rest_4e.h`, `rest_4e_callees.h`, shadow name `rest_4e`): the cut's 48 rows for
R4E (`analysis/round14_cut.tsv`), every one a function - none a jump-table
case or a shared tail, none missing (section 5). Each read to its last
instruction with capstone and fuzzed through the scenario harness's **field**
mode: 288,000 rounds, **0 mismatches**; **153 controls: 145 refused by a
count, one an equivalent mutant with a refused near variant, seven stopped by
ours' own abort, each with a refused near variant** (section 6).
`BOF3X_SHADOW='*'` exit 0, narrow and with `BOF3X_WIDE=1` (section 4). Seven `.data` tables named. **Fuzz
only**: no recorded route enters the faerie village (section 9). No harness
row edited, no raw reference rebound (section 10). No divergence and nothing
for the ledger; one abort the coordinator should weigh (section 7, L5).

**What the band is.** The community band (`0x452DD0..0x464B60`, the faerie
village, areas 175..185; [`labelling-pass.md`](labelling-pass.md) section 5)
is resident code on the PC. This group's 48 are reached from the field
through two tables whose readers are R4B's (`0x652A70` and `0x652AE4`, both
jumped through in R4B's host `0x456E40`: `0x652A70[11]` is
`CommuMusic_Dispatch`, `[18]` `CommuItem_Frame`, `0x652AE4[2]`
`CommuRank_Dispatch`) and by R4B's and R4D's calls and tail jumps. By the
code, five things:

- **Panels and pieces** for two name lists: an *entry* of the 60-entry table
  `0x9046D0` (8 bytes: +0 in use, +1 a kind, +2 / +3 an item and its tab, +4
  a dword) with its 60 five-byte names at `0x9048F0`
  ([`save-interchange.md`](save-interchange.md) section 2a), and a *member*
  of the seven `CharacterRecords`. A panel each (`CommuEntry_DrawPanel` with
  its frame and four colour bars, `CommuMember_DrawPanel` with a portrait),
  every present member's panel, a selection frame, a small pointer, a
  9-slice frame of 8 x 8 cells (`Commu_DrawTiledFrame`, PSX twin
  `0x801D9C44` in `COMMU02`), and the count and n-th of the present members.
- **A random name and the two name commits**: `CommuName_MakeRandom` joins
  two halves picked by `Rand` from script messages `0x1F0..0x22F` and
  `0x230..0x26F`; `CommuName_CommitMember` / `CommuName_CommitEntry` (the two
  commit paths [`save-interchange.md`](save-interchange.md) section 2a found)
  write the edited name into the member or the entry the cursor `0x675F8C`
  names, with the old name into `Text_Records` for the message `0xF5`.
- **A track list** (`CommuMusic_*`, twins `0x801D1190` / `0x801D125C` in
  `COMMU03`): three modes on `0x939A3E` - open (the seven members' names into
  `Text_Records`, the playing track kept, a slide in), browse (the row over
  the entry's kind's track count, `0x20` plays or stops the row's track of
  the 40 at `0x653098`, `0x40` closes) and close (the kept track back, a
  slide out, message `0x5A`) - each with its steps on `0x939A40`.
- **An item handed to the entry `0x9039F5`** (`CommuItem_*`, twins
  `0x801D0D4C`, `0x801D11DC`, `0x801D12C8` in `COMMU04`): message `0x60`,
  an inventory window (WindowRecords 0 and 1, four tabs, pages of nine, a help
  line), cancelled (message `0x5F`) or chosen: an item whose table byte has
  bit 3 is refused (message `0xEB`, then `0x63`), else its name to
  `Text_Records` and message `0x61`, then the item removed from the
  inventory and recorded in the entry (+2, +3 with bit 4, +4 the dword
  `0x904134`).
- **Three ranked lists paged through** (`CommuRank_*`): the counts
  `0x9039A0` (rows `0x9039C0..`, 7 a page), `0x904A90` (`0x904F00..`, 20 a
  page) and `0x937F80` (pairs `0x904CA0..`, 8 a page) - the chain
  `0x455540..0x456080` (R4A's) computes them on an area change - turned into
  page counts, a tiled backdrop, each list's heading and rows, a page number
  over the total.

The cut's PSX twins (`analysis/pairs_propagated.json`) are in the `COMMU02`..
`COMMU04` overlays, which the sibling leaves unnamed (`names/overlays.toml`
has the overlays, `symbols.toml` no function there): **no name was
transferred**. The prefix `Commu` is the overlays' file name and the band's,
a label and not a claim about play; every name is from what the code does.
**What the screens are in the game is the owner's to say.**

| Function | Address | Size | Reached by | What |
|---|---|--:|---|---|
| `CommuEntry_DrawPanel` | `0x45E870` | 0x140 | R4D's `0x45D0D0..0x45D9E0` (14 E8) | box `0x48 x 0x26`, frame, the entry's name or the edited one, a black 60 x 16 TILE, four bars |
| `CommuEntry_DrawPanelFrame` | `0x45E9B0` | 0xA1 | `CommuEntry_DrawPanel` | pieces 6..13 round the panel |
| `CommuEntry_DrawBar` | `0x45EA60` | 0x19E | `CommuEntry_DrawPanel` (4) | two POLY_G4s, dim / bright / dim, colour 0..3 from a stack table |
| `Commu_DrawPiece6` | `0x45EC00` | 0xBF | the frame (16), R4D's `0x45E700` / `0x45E770` (9) | a SPRT of the 6-byte piece table `0x652EE4` |
| `CommuCursor_DrawArrow` | `0x45ECC0` | 0xA1 | R4B's `0x4577A0..0x458410` (11), R4D's `0x45D170`, `0x45D290` | a triangle pointing down, green steady or pulsing |
| `CommuName_MakeRandom` | `0x45ED70` | 0x9F | R4D's `0x45D4E0`, `0x45E060` | two random message halves into `0x675F98`; eax length + 1 |
| `CommuMember_DrawPanel` | `0x45EE10` | 0x9A | `CommuMember_DrawAll`, R4D's (15 E8) | box `0x7D x 0x2D`, portrait, the member's name or the edited one, pieces |
| `CommuMember_DrawPortrait` | `0x45EEB0` | 0xDE | `CommuMember_DrawPanel` | a `0x28 x 0x30` SPRT of the 4-byte table `0x652F38`, three shades |
| `CommuMember_DrawAll` | `0x45EF90` | 0x61 | R4D's `0x45DB90`, `0x45DD10` | every present member's panel, two to a row |
| `CommuMember_Count` | `0x45F000` | 0x1D | R4D's `0x45DB90` | al = present members |
| `CommuMember_Nth` | `0x45F020` | 0x2C | `CommuName_CommitMember` (9), R4D's (15) | al = the n-th present member, `0xFF` none |
| `CommuMember_DrawFrame` | `0x45F050` | 0x14D | R4D's `0x45DB90`, `0x45DD10` | LINE_F3 + LINE_F4 round a panel, channels on or pulsing |
| `Commu_DrawTiledFrame` | `0x45F1A0` | 0x3F7 | R4D's `0x45D610..0x45E350` (6) | five windowed SPRTs and four corner pieces |
| `CommuName_CommitMember` | `0x45F5A0` | 0xB0 | R4D's `0x45E130`, `0x45E520` (jmp) | the edited eight bytes into the member's name |
| `CommuName_CommitEntry` | `0x45F650` | 0xF7 | R4D's `0x45D5B0`, `0x45D990` (jmp) | the edited five bytes into the entry's name |
| `CommuMusic_Dispatch` | `0x45F750` | 0xE | `0x652A70[11]` | jmp through `CommuMusic_Modes` by `0x939A3E` |
| `CommuMusic_OpenDispatch` | `0x45F760` | 0xE | `CommuMusic_Modes[0]` | jmp through `CommuMusic_OpenSteps` by `0x939A40` |
| `CommuMusic_Open` | `0x45F770` | 0x58 | `CommuMusic_OpenSteps[0]` | members' names into `Text_Records`, the track kept, row 0, slide 4 |
| `CommuMusic_SlideIn` | `0x45F7D0` | 0x47 | `CommuMusic_OpenSteps[1]` | slide - 1, the list at `0x28 - 50 slide`; 0: browse |
| `CommuMusic_BrowseDispatch` | `0x45F820` | 0xE | `CommuMusic_Modes[1]` | jmp through `CommuMusic_BrowseSteps` |
| `CommuMusic_Browse` | `0x45F830` | 0x11F | `CommuMusic_BrowseSteps[0]` | the row moved, a track played or stopped, or close |
| `CommuMusic_Play` | `0x45F950` | 0x37 | `CommuMusic_BrowseSteps[1]` | once loaded, `Music_Play(track, 8)` |
| `CommuMusic_CloseDispatch` | `0x45F990` | 0xE | `CommuMusic_Modes[2]` | jmp through `CommuMusic_CloseSteps` |
| `CommuMusic_CloseFade` | `0x45F9A0` | 0x34 | `CommuMusic_CloseSteps[0]` | another track playing: fade it out |
| `CommuMusic_CloseRestore` | `0x45F9E0` | 0x3E | `CommuMusic_CloseSteps[1]` | once loaded, the kept track played again |
| `CommuMusic_SlideOut` | `0x45FA20` | 0x51 | `CommuMusic_CloseSteps[2]` | slide + 1; at 4 message `0x5A` |
| `CommuMusic_DrawList` | `0x45FA80` | 0x255 | the eight music states (10 E8) | the list window, the row's title, two labels, four glyph strings, two quads |
| `CommuMusic_DrawLabel` | `0x45FCE0` | 0x120 | `CommuMusic_DrawList` (2) | a string in 8 x 8 font quads, slanted |
| `CommuItem_Frame` | `0x45FE00` | 0x1D | `0x652A70[18]` | call through `CommuItem_Modes` by `0x939A3E`; then `Field_RunTaskRecords` |
| `CommuItem_StepDispatch` | `0x45FE20` | 0xE | `CommuItem_Modes[0]` | jmp through `CommuItem_Steps` by `0x939A40` |
| `CommuItem_Prompt` | `0x45FE30` | 0x27 | `CommuItem_Steps[0]` | message `0x60` |
| `CommuItem_OpenWindow` | `0x45FE60` | 0x27 | `CommuItem_Steps[1]` | the windows reset and set up, slide 4 |
| `CommuItem_Choose` | `0x45FE90` | 0x338 | `CommuItem_Steps[2]` | the inventory window: tabs, rows, pages; cancel or an item chosen |
| `CommuItem_Cancel` | `0x4601D0` | 0x91 | `CommuItem_Steps[3]` | the window away; message `0x5F` |
| `CommuItem_Confirm` | `0x460270` | 0x172 | `CommuItem_Steps[4]` | the window away; refused (`0xEB`) or the name and message `0x61` |
| `CommuItem_Give` | `0x4603F0` | 0x66 | `CommuItem_Steps[5]` | the item removed, recorded in the entry |
| `CommuItem_Refused` | `0x460460` | 0x1B | `CommuItem_Steps[6]` | message `0x63` |
| `CommuItem_SetupWindow` | `0x460480` | 0x72 | `CommuItem_OpenWindow` | WindowRecords 0 and 1 |
| `CommuRank_Dispatch` | `0x460500` | 0xE | `0x652AE4[2]` | jmp through `CommuRank_Modes` by `0x939A3E` |
| `CommuRank_Pages` | `0x460510` | 0xB1 | `CommuRank_Modes[0]` | the three lists' page counts |
| `CommuRank_Show` | `0x4605D0` | 0x14C | `CommuRank_Modes[1]` | the page's list, paging, the page number; confirm / cancel closes |
| `CommuRank_Close` | `0x460720` | 0x7 | `CommuRank_Modes[2]` | `0x9039F4` + 1 |
| `CommuRank_DrawListA` | `0x460730` | 0x155 | `CommuRank_Show` | heading, up to 7 names with one of two messages |
| `CommuRank_DrawIcon` | `0x460890` | 0x85 | the two headings | a 12 x 12 icon |
| `CommuRank_DrawListB` | `0x460920` | 0x119 | `CommuRank_Show` | heading, up to 20 names three to a row |
| `CommuRank_DrawListC` | `0x460A40` | 0xE0 | `CommuRank_Show` | up to 8 rows: a kind's message, with an entry's name |
| `CommuRank_DrawBackdrop` | `0x460B20` | 0x119 | `CommuRank_Show` | the tiled backdrop |
| `CommuRank_DrawTile` | `0x460C40` | 0x6E | `CommuRank_DrawBackdrop` (4) | a 16- or 32-wide tile |

## 1. The tables named

Each count is the reader's reach read by hand: the tables are nested (a
mode's steps follow its mode table), so each ends where the next begins, and
every entry's code was read to see which value it leaves in the index.

| Table | Address | Count | Reader | Entries |
|---|---|--:|---|---|
| `CommuMusic_Modes` | `0x6530C0` | 3 | `CommuMusic_Dispatch` by `0x939A3E` | the three step dispatchers |
| `CommuMusic_OpenSteps` | `0x6530CC` | 2 | `CommuMusic_OpenDispatch` by `0x939A40` | `CommuMusic_Open`, `_SlideIn` |
| `CommuMusic_BrowseSteps` | `0x6530D4` | 2 | `CommuMusic_BrowseDispatch` | `CommuMusic_Browse`, `_Play` |
| `CommuMusic_CloseSteps` | `0x6530DC` | 4 | `CommuMusic_CloseDispatch` | `_CloseFade`, `_CloseRestore`, `_SlideOut`, R4D's `0x45E6A0`; a text pointer follows |
| `CommuItem_Modes` | `0x653150` | 2 | `CommuItem_Frame` (a `call`) by `0x939A3E` | `CommuItem_StepDispatch`, R4D's `0x45E6A0` |
| `CommuItem_Steps` | `0x653158` | 7 | `CommuItem_StepDispatch` by `0x939A40` | `_Prompt` .. `_Refused` |
| `CommuRank_Modes` | `0x653174` | 3 | `CommuRank_Dispatch` by `0x939A3E` | `_Pages`, `_Show`, `_Close`; the words of `0x653180` follow |

Every reader is unchecked; ours aborts past the count (section 7). The run of
code pointers from `0x6530C0` is 11 and from `0x653150` 12 (the tool's
counts): they run into the nested tables. Not named here (their readers are
R4B's): `0x652A70` / `0x652A84` (five-entry rows of facility handlers, entries
11 and 18 ours) and `0x652AE4` (by `0x9039F4`, entry 2 ours). Image tables read
in place and not entered (no function of ours jumps through them): the piece
table `0x652EE4`, the portraits `0x652F38`, the member panel's piece list
`0x652F5C`, the tracks `0x653098`, the label pointers `0x6530F0..F8` and glyph
strings `0x653144..4C`, the kind messages `0x653180`, the backdrop's tiles
`0x6531A0..0x6531F3`, the page format `0x6531F4`, the bars `0x653210`.

## 2. Divergence

None. Every function is a faithful replacement; where the original indexes
past what it means to (section 7) ours aborts, the round's rule, with no
ledger entry.

## 3. Arguments and answers

- `CommuName_MakeRandom` answers the whole eax (the length + 1: R4D's callers
  copy that many bytes); `CommuMember_Count` and `CommuMember_Nth` answer al
  (`ret_mask 0xFF`; the rest of eax the caller's in `Count`, the loop's index
  in `Nth`). Everything else is void.
- **The draws read their coordinates as low words** (signed, `movsx`) and
  their indexes as bytes; several callers (R4D's, and the group's own
  `CommuMember_DrawAll`, `CommuMusic_SlideIn` / `SlideOut`,
  `CommuItem_Choose`) push dwords with leftovers above those, and the stand-ins
  compare what each callee reads. Two read their position **unsigned**:
  `CommuMember_DrawPortrait` (both words zero-extended before `fild`) and
  `CommuMember_DrawFrame` (x + 2 and y masked with `0xFFFF`).
- **The originals write their own argument slots** as `fild` scratch
  (`CommuEntry_DrawPanel`, `CommuMember_DrawPortrait`, `CommuMember_DrawFrame`
  builds its colour there, `Commu_DrawTiledFrame`, `CommuCursor_DrawArrow`,
  `CommuMusic_DrawList`); their callers pop them unread. Ours does not.
- `Item_NamePtr` is handed the entry's +3 and +2 bytes in dwords whose upper
  bytes are the original's leftovers (`edx` after `Window_ResetAll`, `eax` =
  8 entry with its low byte replaced); it reads the bytes. `Inventory_Remove`
  likewise, with a fourth 0 pushed. `Item_HelpMessage` and the list draws'
  page arguments the same.

## 4. The fuzz (`rest_4e_fuzz.cpp`)

`scenario_harness::Run` in field mode, 6,000 rounds a function, the
facility's modes and steps `kState` (the seven dispatchers' bytes seeded below
their own tables), the draws and helpers `kCall` (`CommuMusic_DrawLabel`'s
text a pointer into the harness's scratch, `Arg::kScratch`).
`BOF3X_R4E_ONLY=<name>` runs the clones whose name contains it.

**Regions beyond field mode's**: the band's cells `0x675F8C` + `0x50`, the
facility bytes `0x939A3C` + 8, `WindowRecords` (22 x `0x24`), the character
records past the style region (`0x903A94` + `0x460`), the save block's
inventory lists `0x904160` + `0x400` and `0x904700` + `0x330` (the entries'
end, the track counts, the names), the lists' counts and rows (`0x904A90`,
`0x937F80`, `0x9039A8` + `0x58`, `0x904F00` + `0x100`), the pairs and
`Text_Records` (`0x904CA0` + `0x140`), the script pool's head and the seeded
strings (`0x803580` + `0xC00`), the confirm / cancel words (`0x903584` +
`0x10`). 31,200 bytes of state in 51 regions.

**Seeds**: the script pool's offsets of every message the group opens or
draws (`0x37..0x3B`, `0xEC`, `0x1F0..0x26F`, `0x271..0x29A`, the kinds' ids
read from `0x653180`) pointed at 96 seeded strings of at most 12 bytes (an
early NUL and spaces often), so a random name's two halves fit the 32 bytes;
the mode and step bytes at 0..6 and `0xFF`, each dispatcher's below its
table; the seven members' presence all, none or random; the entry `0x9039F5`
below 60 (two functions write through it) with its kind 0..8 and each kind's
track count at 0, 1, 2, `0x26`, `0x27`; the row at 0, 1, the count, one
either side, 39, -1, -2; the slide 0..5 and `0xFF`; `Music_Track` `0xFF`, the
row's track or any, the kept one likewise; window 0 shut or not, the tab
0..4, the top and row at the paging bounds (0, 1, 8, 9, `0x6E`, `0x6F`,
`0x76`, `0x77`; top - 1, top + 8, top + 9, `0x7E`, `0x7F`), the scroll word
mostly 0, the row's item empty half the time; the three lists' counts at their
page bounds, the pairs' kinds below 8 and entries below 60 for the four pages
the counts reach; the page counts 0..13 with the page below their sum (the
walk aborts past it); the pad's pressed word at each branch's bits, the
confirm and cancel words; `Field_Request` 0, 1, 2; the label's text with
spaces and a NUL. **Arguments**: the panels' entry and member indexes near
their ends, `editing` 0, 1, `0x100`; the bar's colour 0..3; the pieces
`CommuEntry_DrawPanelFrame` and R4D use; the portrait 0..8 and its shade 0,
1, 2, `0x100`, `0x101`; the steady / pulse flags 0, 1, `0x100`; the pages
0..3; the tile's width 0, 1, `0x100`.

**Stand-ins** (the group's listing; masks by what each callee reads): the
group's own callees with the low words of x and y and the bytes of indexes;
`CommuMember_Nth` answers 0..6 and R4D's `0x45E6D0` 0..59 (the ends a third
of the time) - the commits abort past them; `0x5A7570` fills the primitive's
0x2C bytes (louder than the real header); `Gpu_SetDrawMode` logs the texture
window's 8 bytes, not its stack address; `Input_AutoRepeat` answers each
branch's bits or the pressed word; `Item_NamePtr` a name in the text buffer;
`Crt_sprintf` writes the third argument's digits at the destination (its
fourth, `CommuRank_Show`'s total, is not logged - the three-argument callers
leave their frame there); `Menu_DrawOutlineNotched`, `Window_ResetAll`,
`Gpu_SetLineF3` / `F4` listed (not field-standard); `Inventory_Remove` each
argument's byte.

**Disturbance** (from the hash alone, `sh::DisturbCase(h, 19)`): the step,
the mode, the two slides, window 0's shut byte, the tab (0..4), the row, the
scroll word, the entry (below 60), `Music_Track`, a byte of the edited name,
one list's count, a row of the first list, the window style, a byte of the
label, the track row (-2..41), the entry's +2 / +3, the kept track or the
page, the name cursor - each a cell a function reads again after a call, and
each case proved live by a control (section 6).

**Result** (this worktree, the committed fuzz): `BOF3X_SHADOW=rest_4e`, exit
0: 288,000 rounds over 48 functions (6,000 each), 2,227,838 calls to the
stand-ins, **0 mismatches**; every callee listed was reached and every entry of the seven
tables (the handler recorders' phase counts 550..3,050 each). The harness
names each clone "outside the field runs" (it does not refuse them).

**`BOF3X_SHADOW='*'`** (this worktree): exit 0, 742 self-test lines, none with
a mismatch (`rest_4e`'s line 192,000 rounds, 0 mismatches, at the 4,000-round
fuzz); with `BOF3X_WIDE=1`: exit 0, 742 lines, none with a mismatch; again at
the committed fuzz (6,000 rounds): see the report. No run died silently.
`tools/ledger_check.py`: 73 entries, 0 errors.

## 5. What the cut and the tool said, settled

- **Every row is a function**: the 26 hidden starts each have their own
  frame and `ret` (or a tail `jmp`) and are reached by address - 24 through
  the seven tables above and `0x652A70` / `0x652AE4`, two by R4D's tail
  jumps (`CommuName_Commit*`, two each). The three hosts the cut names
  (`0x45F1A0` over 13 of them, `0x45FCE0` over 9, `0x460480` over 4) end at
  their own `ret`s: `0x3F7`, `0x120`, `0x72` bytes. None is a jump-table case
  or a shared tail.
- **No code no list has**: the tool found none in the band, and the read
  agrees - every byte between the 48 extents is `nop` padding.
- **Every extent is the tool's** (8,963 bytes; 25 differ from the cut by
  padding only, none by code).
- The cut's labels ("minigames, master", "field core", `field_c1`) and the
  `hypothesis` rows (`0x4603F0`, `0x460460`, `0x460480`, `0x460B20`,
  `0x460C40`) were hints: all five are functions of the facilities above.

## 6. Controls

153 plants, each in `rest_4e.cpp` by a unique anchor: the scratch script
`controls.py` (session `309e3952` scratchpad, `r4e/`, with `controls_list.py`)
plants, rebuilds, runs `BOF3X_SHADOW=rest_4e` with `BOF3X_R4E_ONLY` naming the
function, restores and rebuilds at the end (the tree clean after). **145
refused by a count** (exit 3, the harness's mismatch Fatal). **C91 is an
equivalent mutant**: it reuses the entry pointer for the second write where
the original reads `0x9039F5` again with no call between the two reads, so no
input can tell them apart; its near variant C153 (the entry read before the
sound `0x105`) is refused. **Seven were stopped by ours' own abort**, not by a
count (a crash is not a refusal): C40, C41, C46, C54, C73, C113 dispatch by
the other facility byte, which the seeds leave unbounded, and C15 points the
second name half at unseeded messages whose strings run past the 32 bytes;
their near variants C146..C152 (the next entry, in range; the first set of
halves) are refused in every round.

**What the controls fixed in the fuzz** (first not refused at 4,000 rounds,
refused after): C87, C90 (disturbance cases 7 and 5, cells `CommuItem_Choose`
reads after its sounds: the function is seeded open, not scrolling and
confirmed two times in three, the scroll case writes a non-zero word) and
C125 (case 12: it now flips every row of the first list, so the row being
drawn moves). Rounds went to 6,000. **Each of the 19 disturbance cases has a
control refused** (the "case n" plants). C1..C145 ran at the 4,000-round
fuzz and their counts are from that run (bar C87, C90, C91, C125, re-run at
the committed fuzz with C146..C153); the fuzz's change only added rounds and
paths.

| Control | Run on | Plant | Rounds refused |
|---|---|---|--:|
| C1 | `CommuEntry_DrawPanel` | box h 0x27 | 4000 |
| C2 | `CommuEntry_DrawPanel` | bar length 11 x | 3951 |
| C3 | `CommuEntry_DrawPanel` | editing inverted | 4000 |
| C4 | `CommuEntry_DrawPanel` | tile y + 0x13 | 4000 |
| C5 | `CommuEntry_DrawPanelFrame` | piece 9 for 8 | 4000 |
| C6 | `CommuEntry_DrawPanelFrame` | left column y + 9 | 4000 |
| C7 | `CommuEntry_DrawBar` | colour 3 red + blue | 1021 |
| C8 | `CommuEntry_DrawBar` | bottom y + 5 | 4000 |
| C9 | `CommuEntry_DrawBar` | dim a half | 1007 |
| C10 | `Commu_DrawPiece6` | clut x >> 3 | 3763 |
| C11 | `Commu_DrawPiece6` | next piece | 3988 |
| C12 | `CommuCursor_DrawArrow` | x + 5 | 4000 |
| C13 | `CommuCursor_DrawArrow` | pulse + 0x3E | 1999 |
| C14 | `CommuCursor_DrawArrow` | steady bit 0 | 376 |
| C15 | `CommuName_MakeRandom` | second half 0x231 | ours aborts (not a refusal) |
| C16 | `CommuName_MakeRandom` | eax the length | 4000 |
| C17 | `CommuName_MakeRandom` | two bytes on bit 6 | 2739 |
| C18 | `CommuMember_DrawPanel` | portrait +8 | 3132 |
| C19 | `CommuMember_DrawPanel` | name count 6 | 4000 |
| C20 | `CommuMember_DrawPortrait` | shade 2 green 0x41 | 1312 |
| C21 | `CommuMember_DrawPortrait` | x signed | 1968 |
| C22 | `CommuMember_DrawAll` | row y 0x2C | 3170 |
| C23 | `CommuMember_DrawAll` | present bit 1 | 3964 |
| C24 | `CommuMember_Count` | flags +0xA | 3517 |
| C25 | `CommuMember_Nth` | none 0xFE | 2561 |
| C26 | `CommuMember_Nth` | index + 1 | 1439 |
| C27 | `CommuMember_DrawFrame` | F3 red and blue swapped | 2006 |
| C28 | `CommuMember_DrawFrame` | right 0x7F | 4000 |
| C29 | `CommuMember_DrawFrame` | x signed | 2050 |
| C30 | `Commu_DrawTiledFrame` | window 2 x 0x99 | 4000 |
| C31 | `Commu_DrawTiledFrame` | middle h - 0x11 | 4000 |
| C32 | `Commu_DrawTiledFrame` | corner 0x28 | 4000 |
| C33 | `Commu_DrawTiledFrame` | clut row 0x1E0 | 4000 |
| C34 | `CommuName_CommitMember` | old byte 7 | 3981 |
| C35 | `CommuName_CommitMember` | member asked once | 4000 |
| C36 | `CommuName_CommitMember` | mode 2 | 4000 |
| C37 | `CommuName_CommitEntry` | every space a NUL | 71 |
| C38 | `CommuName_CommitEntry` | NUL as 0x21 | 82 |
| C39 | `CommuName_CommitMember` | cursor read once (case 18) | 66 |
| C40 | `CommuMusic_Dispatch` | by the step | ours aborts (not a refusal) |
| C41 | `CommuMusic_OpenDispatch` | by the mode | ours aborts (not a refusal) |
| C42 | `CommuMusic_Open` | slide 5 | 4000 |
| C43 | `CommuMusic_Open` | name byte 9 | 4000 |
| C44 | `CommuMusic_SlideIn` | slide not read again (case 2) | 3 |
| C45 | `CommuMusic_SlideIn` | y 0x29 | 4000 |
| C46 | `CommuMusic_BrowseDispatch` | by the mode | ours aborts (not a refusal) |
| C47 | `CommuMusic_Browse` | wrap at the count | 157 |
| C48 | `CommuMusic_Browse` | up wraps to count - 1 | 290 |
| C49 | `CommuMusic_Browse` | pad read once | 16 |
| C50 | `CommuMusic_Browse` | track read before the fade (case 9) | 1 |
| C51 | `CommuMusic_Browse` | count by +2 | 498 |
| C52 | `CommuMusic_Play` | frames 9 | 2701 |
| C53 | `CommuMusic_Play` | step 1 | 2694 |
| C54 | `CommuMusic_CloseDispatch` | by the mode | ours aborts (not a refusal) |
| C55 | `CommuMusic_CloseFade` | none 0xFE | 374 |
| C56 | `CommuMusic_CloseFade` | step + 2 | 4000 |
| C57 | `CommuMusic_CloseRestore` | kept read before the load (case 17) | 4 |
| C58 | `CommuMusic_CloseRestore` | slide 1 | 2694 |
| C59 | `CommuMusic_SlideOut` | slide not read again (case 2) | 3 |
| C60 | `CommuMusic_SlideOut` | message 0x5B | 501 |
| C61 | `CommuMusic_DrawList` | colour 1 | 2051 |
| C62 | `CommuMusic_DrawList` | style read once (case 13) | 39 |
| C63 | `CommuMusic_DrawList` | labels swapped | 4000 |
| C64 | `CommuMusic_DrawList` | second quad 1 wide | 4000 |
| C65 | `CommuMusic_DrawList` | v3 0xE1 | 4000 |
| C66 | `CommuMusic_DrawList` | title 0x270 + row | 3975 |
| C67 | `CommuMusic_DrawLabel` | v by shift | 3234 |
| C68 | `CommuMusic_DrawLabel` | slant 8 | 3898 |
| C69 | `CommuMusic_DrawLabel` | skip 0x21 | 3646 |
| C70 | `CommuMusic_DrawLabel` | clut & 0x7F | 1928 |
| C71 | `CommuMusic_DrawLabel` | byte read before the call (case 14) | 5 |
| C72 | `CommuItem_Frame` | tasks unless 2 | 1561 |
| C73 | `CommuItem_StepDispatch` | by the mode | ours aborts (not a refusal) |
| C74 | `CommuItem_Prompt` | message 0x62 | 2477 |
| C75 | `CommuItem_Prompt` | open unless 1 | 2325 |
| C76 | `CommuItem_OpenWindow` | slide 3 | 2477 |
| C77 | `CommuItem_OpenWindow` | no window set up | 2477 |
| C78 | `CommuItem_Choose` | cursor x + 8 | 2373 |
| C79 | `CommuItem_Choose` | cursor rows of 12 | 2370 |
| C80 | `CommuItem_Choose` | left wraps to 2 | 180 |
| C81 | `CommuItem_Choose` | right past 4 | 162 |
| C82 | `CommuItem_Choose` | scroll up at the top | 17 |
| C83 | `CommuItem_Choose` | scroll down at 8 | 17 |
| C84 | `CommuItem_Choose` | page up below 8 | 46 |
| C85 | `CommuItem_Choose` | page down past 0x6F | 13 |
| C86 | `CommuItem_Choose` | sound when still | 2373 |
| C87 | `CommuItem_Choose` | scroll read before the sound (case 7) | 3 |
| C88 | `CommuItem_Choose` | cancel step + 2 | 240 |
| C89 | `CommuItem_Choose` | item + 1 | 214 |
| C90 | `CommuItem_Choose` | tab read before the sound (case 5) | 1 |
| C91 | `CommuItem_Choose` | entry read once (case 8) | 0 (equivalent: section 6) |
| C92 | `CommuItem_Choose` | help row not read again (case 6) | 3 |
| C93 | `CommuItem_Choose` | help x 0x1E | 1017 |
| C94 | `CommuItem_Choose` | step 5 | 213 |
| C95 | `CommuItem_Choose` | slide stops at 1 | 662 |
| C96 | `CommuItem_Choose` | tab move 0x33 | 608 |
| C97 | `CommuItem_Choose` | outline slide not read again (case 3) | 5 |
| C98 | `CommuItem_Cancel` | window read before the frame (case 4) | 8 |
| C99 | `CommuItem_Cancel` | message 0x5E | 2373 |
| C100 | `CommuItem_Cancel` | mode read before the message (case 1) | 6 |
| C101 | `CommuItem_Confirm` | bit 2 | 293 |
| C102 | `CommuItem_Confirm` | armour +0x10 | 168 |
| C103 | `CommuItem_Confirm` | name 15 bytes | 2117 |
| C104 | `CommuItem_Confirm` | refused step 7 | 244 |
| C105 | `CommuItem_Confirm` | entry read before the reset (case 8) | 3 |
| C106 | `CommuItem_Give` | bit 5 | 2428 |
| C107 | `CommuItem_Give` | dword + 1 | 2477 |
| C108 | `CommuItem_Give` | entry read once (case 8) | 8 |
| C109 | `CommuItem_Give` | mode + 2 | 2477 |
| C110 | `CommuItem_Refused` | message 0x64 | 2477 |
| C111 | `CommuItem_SetupWindow` | x -0xA9 | 4000 |
| C112 | `CommuItem_SetupWindow` | window 1 +2 2 | 4000 |
| C113 | `CommuRank_Dispatch` | by the step | ours aborts (not a refusal) |
| C114 | `CommuRank_Pages` | 19 a page | 1919 |
| C115 | `CommuRank_Pages` | n / per + 1 | 3107 |
| C116 | `CommuRank_Show` | backdrop y 0x19 | 4000 |
| C117 | `CommuRank_Show` | wrap past the total | 197 |
| C118 | `CommuRank_Show` | back wraps to the total | 395 |
| C119 | `CommuRank_Show` | page read once (case 17) | 3 |
| C120 | `CommuRank_Show` | confirm only | 489 |
| C121 | `CommuRank_Show` | number + 2 | 4000 |
| C122 | `CommuRank_Show` | list A for 1 | 2845 |
| C123 | `CommuRank_Close` | + 2 | 4000 |
| C124 | `CommuRank_DrawListA` | message 0x38 for 0x39 | 1892 |
| C125 | `CommuRank_DrawListA` | entry read once (case 12) | 28 |
| C126 | `CommuRank_DrawListA` | count read once (case 11) | 11 |
| C127 | `CommuRank_DrawListA` | icon + 1 | 4000 |
| C128 | `CommuRank_DrawIcon` | u - 0x37 | 4000 |
| C129 | `CommuRank_DrawIcon` | clut 0x784A | 4000 |
| C130 | `CommuRank_DrawListB` | 20 a page | 930 |
| C131 | `CommuRank_DrawListB` | x + 0x13 | 1754 |
| C132 | `CommuRank_DrawListB` | count read once (case 11) | 16 |
| C133 | `CommuRank_DrawListC` | row y + 0x1D | 1738 |
| C134 | `CommuRank_DrawListC` | name when 0 | 1712 |
| C135 | `CommuRank_DrawListC` | name byte 3 | 1585 |
| C136 | `CommuRank_DrawListC` | count read once (case 11) | 4 |
| C137 | `CommuRank_DrawBackdrop` | columns 63 apart | 4000 |
| C138 | `CommuRank_DrawBackdrop` | bottom y + 0xB1 | 4000 |
| C139 | `CommuRank_DrawBackdrop` | v & 0xFF | 4000 |
| C140 | `CommuRank_DrawTile` | wide bit 0 | 1011 |
| C141 | `CommuRank_DrawTile` | clut 0x7884 | 4000 |
| C142 | `CommuName_CommitMember` | edited bytes read once (case 10) | 39 |
| C143 | `CommuMusic_Play` | row read before the load (case 15) | 13 |
| C144 | `CommuItem_Give` | tab read before the removal (case 16) | 14 |
| C145 | `CommuItem_Prompt` | step read before the message (case 0) | 5 |
| C146 | `CommuMusic_Dispatch` | the next mode | 6000 |
| C147 | `CommuMusic_OpenDispatch` | the next step | 6000 |
| C148 | `CommuMusic_BrowseDispatch` | the next step | 6000 |
| C149 | `CommuMusic_CloseDispatch` | the next step | 6000 |
| C150 | `CommuItem_StepDispatch` | the next step | 6000 |
| C151 | `CommuRank_Dispatch` | the next mode | 6000 |
| C152 | `CommuName_MakeRandom` | second half from the first set | 5922 |
| C153 | `CommuItem_Choose` | entry read before the sound (case 8) | 10 |


## 7. Latent defects and ranges (Capcom's, described, not fixed)

**The group's one policy for unbounded indexes.** Ours aborts with a message
where the original's index would jump through, write through, or take a
pointer from memory past the table it means: the seven dispatchers (past
each count, section 1); the two name commits when the member or entry search
answers `0xFF` (the original writes `0xA4 x 255` past the member records, or
`5 x 255` past the names); `CommuItem_Choose` and `CommuItem_Give` when the
entry `0x9039F5` is 60 or more (a write past the 60 entries) and
`CommuItem_Choose` when the tab is 5 or more (a pointer read past
`Inventory_IdLists`' five); `CommuEntry_DrawBar` past colour 3 (its colour
table is on its own stack: the original reads its frame); `CommuName_MakeRandom`
when the two halves pass the 32 bytes it cleared; `CommuRank_Show` past the
three lists (L5). Where the original only **reads** a byte or a word past an
image or save table to draw it - a piece past 14, a portrait past 9, a track
row outside 0..39, a bar length or a name past entry 59, a kind past 8, a
member past 6, the entry's kind past the eight track-count records - ours
reads the same bytes, as `Item_NamePtr` already does for the item tables.

- **L1 - `CommuMusic_DrawList`'s two quads have no width.** Each POLY_FT4's
  right edge is its left plus an int the original keeps on its stack and
  zeroes before the loop (both of them 0): both quads (u `0x98..0x9C`, v
  `0xD8..0xE0`, at `(x + 0x49, y + 0x3F + 4 k)`) are vertical lines. Ours
  draws the same. Whether something was meant to show there is the owner's.
- **L2 - `CommuRank_DrawListB` pages by 15 rows of data, 20 on screen.**
  The page is counted 20 entries at a time (the page count is
  `(n - 1) / 20 + 1`, the loop stops at 20 page + i), but the names come from
  `0x904F00 + 15 page + i`: page 1 starts with page 0's last five, and with
  more than 20 entries the last of them are never shown. Ours draws the same.
- **L3 - `CommuName_MakeRandom` does not bound its copy** into the 32 bytes
  of `0x675F98` (two messages, each to its NUL); ours aborts where it would
  pass them. The real halves are the script pool's; none was measured.
- **L4 - coordinates as unsigned words**: `CommuMember_DrawPortrait` places
  a portrait at x or y `0xFFFF` for -1 (`fild` of the zero-extended word);
  `CommuMember_DrawFrame` likewise for x + 2 and y. Ours draws the same.
- **L5 - `CommuRank_Show`'s walk** subtracts the three page counts from the
  page until one is above what is left, unbounded: with all three lists
  empty (every count 0) it reads on past `0x675FDA` into the page byte and
  `EffectKind1E_ShardCursor` `0x675FDC` and, should one of those be above 0,
  draws the third list's page (empty) with that index - else it walks on.
  **Ours aborts.** Whether play opens this screen with all three lists empty
  is not known (`CommuRank_Pages` runs first and leaves the counts 0 for an
  empty list); if it does, the original's nearest answer is the third list
  drawn empty with the page number `1/0`. The coordinator's and the owner's.
- **Stale stack, without effect**: `Commu_DrawTiledFrame` computes two sizes
  and the corners' x from a dword whose upper half it never wrote (the word
  stores and `Menu_DrawPiece`'s low word drop it); `CommuRank_Show` keeps the
  list and the page in stack bytes whose upper three it never writes (each
  callee reads the byte).

## 8. Calls across groups, inbound

Raw calls out (`rest_4e_callees.h`; the round's rebinding turns them into
names): R4D's `0x45E6D0` (the n-th in-use entry, from
`CommuName_CommitEntry`, four sites) and `0x5A7570` (nobody's: the library
layer's POLY_F3 header, from `CommuCursor_DrawArrow`). R4D's `0x45E6A0` is an
entry of two of the group's tables, read in place. R2B's
`Menu_DrawOutlineNotched` (7 sites) is merged and called by name.

Inbound from outside the group (for the rebinding pass): R4B's `0x4577A0`,
`0x457990`, `0x457AE0`, `0x457CE0`, `0x457DD0`, `0x457FE0`, `0x458240`,
`0x458410` (11 calls of `CommuCursor_DrawArrow`) and its host `0x456E40`'s
three table reads (`0x652A70[11]`, `[18]`, `0x652AE4[2]`); R4D's 69 sites
(`band_rows.py --edges`): `CommuEntry_DrawPanel` 14, `CommuCursor_DrawArrow`
2, `CommuName_MakeRandom` 2, `CommuMember_DrawPanel` 15, `CommuMember_Count`
1, `CommuMember_Nth` 15, `CommuMember_DrawAll` 2, `CommuMember_DrawFrame` 2,
`Commu_DrawTiledFrame` 6, `Commu_DrawPiece6` 9 (from `0x45E700`, `0x45E770`),
and four tail jumps to the commits.

## 9. The live route

None: no recorded route enters the faerie village, the catalogue's reach
column is empty for all 48 rows, and no file under `analysis/calltrace`
names any of them. **Fuzz only.** The owner records a route once the band is
ours; the state hash after the merge covers whatever its routes enter.

## 10. The rebinding

`band_rows.py --refs --group R4E` and `grep -rn -i` of the 48 over
`src/game`: one hit, `area_w4d_callees.h:49`, a comment naming the range
`0x456AF0..0x460480` (not a reference to `0x460480`): **nothing rebound,
nothing left raw.** No harness row lists any of the 48
(`scenario_harness*.cpp`, `boss_harness*.cpp`, `area_harness.cpp` grepped),
and no file of another group of this round refers to them (R4B's and R4D's
are not merged). `DIVERGENCE.md`, `cheats.cpp`, `widescreen.cpp`,
`labels.cpp` and `yes_no_layout.cpp` name none of the 48 (no patched
operand); the band has no full-frame fill.

## 11. For `analysis/calltrace/entries_logic.txt`

30 lines appended to the main checkout's file (2026-10-05): the 26 hidden
starts it lacked, and `0045F1A0 3F7`, `0045FCE0 120`, `00460480 72`,
`00460C40 6E` beside the longer host lines it had; the other 18 were already
listed with the extents read here. Left for the round's end (a host's longer
extent over functions of this group): `0045F1A0 8D1` (over the 13 starts
`0x45F5A0..0x45FA20`), `0045FCE0 79B` (over the nine `0x45FE00..0x460460`),
`00460480 2A7` (over the four `0x460500..0x460720`), `00460C40 AAB` (past
`CommuRank_DrawTile`'s `ret` over R4F's `0x460CB0`).
