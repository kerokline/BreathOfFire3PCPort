# Group BE7: the battle windows (`0x597FC0..0x59DB61`)

**Status:** MEASURED (2026-09-29) - round twelve
([`takeover-queue-field-battle.md`](takeover-queue-field-battle.md) section
3), wave one, stage B. All 31 functions the cut table
(`analysis/round12_cut.tsv`, group BE7) lists are ours
(`src/game/battle_e7.cpp`, shadow name `battle_e7`), each read to its last
instruction with capstone and fuzzed through the boss harness as an engine
group ([`boss_harness.md`](boss_harness.md) section 10) without edits to it:
one `Run`, 186,000 rounds, 0 mismatches. CONTROLS_SUMMARY Five of the 31 are
entered by the owner's `dragonTransform` recipe (section 9); the rest are
fuzz only.

Names are from the code and its callers: `pairs_propagated.json` pairs most
of the band with PSX addresses (`0x801EFE60..0x801F093C` for the result
windows, `0x800AD3C0..0x800B025C` for the rest), but the sibling's corpus
names none of those twins, so no name is transferred. Nothing here states
what a gene or a form does in the game: "gene", "form" and "cost" are the
docs' and the code's words for the bytes the windows read.

| Part | Reached by | Functions |
|---|---|--:|
| The result screen | `Window_Handler4Kinds` `0x597F60` (ours, group CM): kind 0 through `0x597FA0`'s one-entry stack table, kind 1; `BattleResultWin_DrawExp` (ours, group CD) | 4 |
| The gene windows | the window-kind handler `0x598890` (Capcom's, in no group): kind 0's slide states `0x5989B0` / `0x5989F0` call the grid's draw, kinds 1..4 are this group's four stack dispatchers | 26 |
| The equipment window | `0x59CC10` (Capcom's: kind 3 of `Window_Handler8Kinds`) | 1 |

## 1. The cut, read against the code

`tools/band_rows.py --group BE7` (with `--clones`, `--refs`, `--edges`):
31 functions of the cut and 0 not listed; 20 extents differ from the cut's
sizes by trailing padding only, none by code (the tool's extents are the
code's, each to its last `ret`, checked by hand). Every start is a function:
the 20 "hidden" ones sit in their hosts' catalogue extents (`0x597FC0` in
`Text_GlyphCount`'s, the rest in the previous start's) but each is reached
by an address of its own - `0x597FC0` and `0x5984B0` by stack-table
immediates of `0x597FA0` / `Window_Handler4Kinds`, the others by the four
dispatchers' immediates - and no host that is ours contains any of their
code as a fall-through (`Text_GlyphCount` ends at `0x597F5C`). No start was
dropped or added.

## 2. What each function does

`record` is the window record `0x905B84` names (`WindowRecords`, 22 of 0x24
bytes: `+3` the state, `+4` / `+6` x / y, `+8` / `+9` the size in cells);
every original re-reads `0x905B84` before each use, and so does ours.

### 2.1 The result screen

| Function | Address | Size | What |
|---|---|--:|---|
| `BattleResultWin_DrawLevelUp` | `0x597FC0` | 0x30A | kind 0 (through `0x597FA0`): the frame `(0x14, 0x28, 0x118, +9)`; per stat 1..6 whose `0x432170(record +0xA, stat)` (BE1's) answers a word not 0, the value again by `Area08_MessageFormat` into its own buffer `0x904D20 + 0x20 (stat - 1)` and the system message `0xC`, `0xD`, 8, 9, `0xA`, `0xB` drawn at `(0x1E, y)`, y from `0x2C` by `0x10`; then bytes `+6` / `+7` of the member's level row (`Char_ExpTable`, 99 rows of 8 bytes per member, by `+0xA` and `+0xB`) as two abilities (`Ability_Records` by `0x654900` into `0x904DE0` / `0x904E00`, messages `0x12` / `0x13`) |
| `BattleResultWin_DrawFrame` | `0x5982D0` | 0x1D8 | `(x, y, w, h)`, each read as a word: page `(0x3C0, 0)`; a semi-transparent `TILE` at `(s16 x, s16 y + 2)` in the window colour (the first word of CLUT shadow row s8 `0x903A5A`); `BattleWin_DrawQuadF4` shapes 8 and 9 at `y` and `y + h + 2`; two added lines (top, left) and two halved (right, bottom) in the colour's three bytes |
| `BattleResultWin_DrawDrops` | `0x5984B0` | 0xB2 | kind 1: the frame at `(+4, +6)`, `0x118` by `+9`; per drop `i` below `0x904AE7` (re-read) whose word `0x904AF4[i]` (`category << 8 | item`) is not 0, its line at `(+4 + 138 (i & 1) + 5, +6 + 13 (i >> 1) + 4)` (each sum a word, the `+5` / `+4` after) with the count `0x904B14[i]` |
| `BattleResultWin_DrawItem` | `0x598750` | 0xB3 | `(x, y, colour, category, item, count, dim)`: nothing for an item or count byte of 0; `Menu_DrawIcon8(x, y + 2, 0x66AF24[Item_IconKind], dim)`; the name (`Item_NamePtr`, `Text_CharCount` characters) at `(x + 10, y)`; a count above 1 by `0x653EC0` into `0x904BA0` in the 8-pixel font at `(x + 0x6D, y + 2)` |

### 2.2 The gene windows (`0x598890`'s kinds)

| Function | Address | Size | What |
|---|---|--:|---|
| `GeneWin_DrawGrid` | `0x598A30` | 0x1A2 | kind 0's draw (called by `0x5989B0` / `0x5989F0` before they slide the record): `GeneWin_DrawFrame(+4, +6, +8, +9)`; three boxes in the window colour; the label `0x66AF38` and the cost `0x904B78` (by `0x64D3EC`) in the 12-pixel font, colour 7 when the menu actor's AP (`+0x9A`) is below the cost; in step 4 (`0x904AA3`) the hand at the first box (`0x904AA6` = 0xFF) or at cell (`0x904AA7`, `0x904AA6`), 30 by 32 pixels |
| `GeneWin_DrawFrame` | `0x598BE0` | 0x1DE | `(x, y, cols, rows)`, the counts bytes: page `(0x340, 0x100)`; piece `0x25` inside, `0x24` / `0x28` along the top and bottom (cells 2..cols-3), `0x2A` / `0x2B` down the sides (rows 2..rows-3), corners `0x23`, `0x26`, `0x27`, `0x29` 16 pixels in from the far edges |
| `GeneWin_ChoiceStates` | `0x598DC0` | 0x2E | kind 1: `call [esp + 4 * +3]` over the next three |
| `GeneWin_ChoiceOpen` | `0x598DF0` | 0x20 | `+4 = 0x5A`, `+6 = -0x17`, `+3 + 1` |
| `GeneWin_ChoiceSlideIn` | `0x598E10` | 0x37 | `+6` (signed) down to `0x29` by `0x10`, then the draw at `(+4, +6)` |
| `GeneWin_ChoiceSlideOut` | `0x598E50` | 0x37 | `+6` up to `-0x17` by `0x10`, then the draw |
| `GeneWin_DrawChoices` | `0x598E90` | 0xCC | `(x, y)`: three boxes `0x2D` by `0x14`, 48 apart, each with its text (the pointers at `0x66A164`, 16 characters) and pieces `0x66AF3C`; with `0x904AA5` bit 7 set the box `0x904AA5 & 0x7F` gets `0x66AF44` and the others colour 7; with it clear, colour 0 and the hand at that box |
| `GeneWin_ListStates` | `0x598F60` | 0x3E | kind 2: `call [esp + 4 * +3]` over the next five |
| `GeneWin_ListOpen` | `0x598FA0` | 0x7E | nine `BattleTask_Create(0, 0xD)`, each slot's `+9` / `+0xA` its row / column 0..2; `+0x14 = 6`, `+8 = 0x10`, `+9 = 0x11`, `+3 + 1` |
| `GeneWin_ListSlideIn` | `0x599020` | 0x24 | the list's draw, then `+4` right to `0x5B` by `0x20` |
| `GeneWin_ListSlideOut` | `0x599050` | 0x22 | the draw, then `+4 - 0x20`; at `+4 <= -0xA5` a tail jump to `Window_FreeCurrent` instead |
| `GeneWin_ListShiftLeft` | `0x599080` | 0x2F | the draw, then `+4 - 0x20` while above `0x11`, held at `0x11` |
| `GeneWin_ListShiftRight` | `0x5990B0` | 0x2F | the draw, then `+4 + 0x20` while below `0x5B`, held at `0x5B` |
| `GeneWin_DrawList` | `0x5990E0` | 0x274 | the frame (title 0); the six rows' form bytes + 1 into `0x939848` and the scroll bar over them (top `+0x12`, total `+0x14`); rows `+0x12 .. +0x12 + 2` of `0x904608` (four bytes: three indices into the cost table `0x64EC9C`, 0xFF none after the first, and a form byte): the row's number + 1 in the 12-pixel font; for a form byte not 0xFF its icon, its label (the system message `0x64ECB0[form & 0x1F]`, small font) and the summed cost (by `0x66AF8C`); each in colour 7 when the menu actor's AP is below the cost. In step 6 but not sub-step 5 (`0x904AA4`) the cursor box at row `+0x10`, else the hand |
| `GeneWin_List2States` | `0x599360` | 0x2E | kind 3: over the next three |
| `GeneWin_List2Open` | `0x599390` | 0x7E | nine tasks `(0, 0xE)`; `+0x14 = 0xC`, `+8 = 0x18`, `+9 = 0x11`, `+3 + 1` |
| `GeneWin_List2SlideIn` | `0x599410` | 0x24 | the second list's draw, then `+4` left to `0x5B` by `0x20` |
| `GeneWin_List2SlideOut` | `0x599440` | 0x22 | the draw, then right by `0x20`; at `+4 >= 0x15B` freed instead |
| `GeneWin_List3States` | `0x599470` | 0x2E | kind 4: over the next three |
| `GeneWin_List3Open` | `0x5994A0` | 0x6B | nine tasks `(0, 0xE)`; `+0x14 = 0xC`, `+3 + 1` (`+8` / `+9` kept) |
| `GeneWin_List3SlideIn` | `0x599510` | 0x2F | the draw, then left by `0x20` while above `0xA3`, held at `0xA3` |
| `GeneWin_List3SlideOut` | `0x599540` | 0x22 | the draw, then right; at `+4 >= 0x143` freed instead |
| `GeneWin_DrawList2` | `0x599570` | 0x201 | the first list's shape over the twelve rows of `0x904620` (marks `0x939850`, title 1): no row number, a row whose form byte is 0xFF skipped whole; the hand unless step 6 sub-step 5; no cursor box |
| `GeneWin_DrawListFrame` | `0x599780` | 0x190 | `(x, y, -, -, title)`: three boxes in the window colour, the title (the 5-byte string `0x66AF64 + 5 title`, centred on `x + 0x46` by six pixels a character), the piece lists `0x66AF4C` / `0x66AF58` and the edges. **The third and fourth words are pushed by both callers and never read** |
| `GeneWin_DrawFormIcon` | `0x599910` | 0xE4 | `(x, y, form)`: page `(0x340, 0x100)` abr 1; a `SPRT` 0x28 by 0x18 of cell `v = 0x66AF70[form]` (u `(v % 6) * 40`, v `(v / 6) * 24`), CLUT `(v * 16, 0x1FA)` |
| `GeneWin_DrawCursorBox` | `0x599A00` | 0x148 | `(x, y, w, h, flash, sides)`: a `LINE_F3` and a `LINE_F4` outlining the box (words), bright 0xFF or, with `flash`, a pulse of `Frame_Counter`, in the colour channels `sides` bits 0..2 name |

### 2.3 The equipment window

| Function | Address | Size | What |
|---|---|--:|---|
| `BattleEquipWin_Draw` | `0x59D640` | 0x521 | `(member, x, y, set, flags, record)`, called with `(0x66972C[0x904065[+0xC]], +4, +6, +0x20, +0xD, record)`: the box; the member's name (`CharacterRecords + 0xA4 member`, 5 characters at most) centred on `x + 0x3E`; four stat labels and, after `Char_RecalcStats`, the stats `+0x24`, `+0x26`, `+0x2A`, `+0x28`; unless `flags` bit 0, `Equip_PreviewSet(member, set, marks, values)` into its stack and per stat a bar (`0x59DB70`, colour `0x11` for mark 4, `0x12` for 1, else `0x13`) and the previewed value in the mark's colour; then the six equipped items (`+0x12..+0x17`, categories `0x66B5BC`, icons `0x66B5B4`): the record's `+0x10 = 0` first; row r in colour 2 on the record's `+0xB`; with the record's `+0xD` bit 1, an item `Item_CanUse(2, the member 0x66972C[0x904065[s8 0x929F06]], ...)` refuses is dimmed in colour 7, and one it allows on row `+0xA` sets `+0x10 = category << 8 | item`; every item not refused is drawn first dimmed in colour 7, then (rows `+0xA` / `+0xB` two pixels higher) in its colour; last the frame's pieces |

## 3. Divergence

None: every function is a faithful replacement, and no `DIVERGENCE.md`
entry or `cheats.cpp` patch names an address of the band (grepped
2026-09-29). Where the original indexes past a table, ours aborts with a
message (the owner's rule, round9 doc section 6; section 7).

## 4. The harness's use, and what it lacks

One `boss_harness::Run` (`src/game/battle_e7_fuzz.cpp`, `Group::engine`),
6,000 rounds a function:

- **Shapes.** The 20 stack-table entries and `0x5984B0` / `0x597FC0` are
  `kWindow`; the four dispatchers `kWindow` with `states` (`state_at` 3,
  3 / 5 / 3 / 3 entries: the record's `+3` drawn inside the table, their
  immediates `Imm`s with a handler recorder each). The eleven draws their
  callers call directly are `kHelper` with this file's words. No function
  answers (`ret_mask` 0 everywhere: every caller discards `eax`).
- **Own callees.** The nine of the group's functions another of them calls
  directly are listed with the masks of what they read; `GeneWin_DrawList`
  / `_DrawList2` (no arguments) as `kPhase` recorders logging `0x905B84`.
  BE1's `0x432170` (two bytes, `kFlag` - the caller tests the word) and
  `0x59DB70` (nobody's) by address.
- **Narrowed standard masks (for the coordinator's fold).** These windows
  push registers whose upper halves are another call's leftovers (`ecx` /
  `edx` after a call, the handler's own `ecx` at entry, an uninitialised
  stack slot under a colour byte), which ours cannot reproduce and no callee
  reads. The standard set's masks are wider than what our implementations of
  those callees read, so the file lists them again, narrowed to what each
  reads: `Menu_DrawBox` (x, y, w, h `U16`; `menu_windows.cpp`),
  `Menu_DrawPiece` / `_DrawPieces` / `_DrawIcon8` (x, y `U16`, id / icon /
  flags bytes), `Menu_DrawHand` (x, y `U16`, `char_stats.cpp`),
  `Menu_DrawScrollBar` (top, rows, total, height bytes; x, y `U16`),
  `Text_DrawAt` / `Text_DrawSmall` (x, y `U16`, colour and count bytes -
  `Text_DrawString` reads their low bytes), `Text_DrawFont12` / `_DrawFont8`
  (colour & 0x3F), `BattleWin_DrawQuadF4` (x, y `S16`, shape and shade
  bytes; `battle_window_draw.cpp`), `BattleWin_DrawLineAdd` / `_DrawLineHalf`
  (the colours' low bytes: `LineBody`), `Item_CanUse` (mode 2 reads bytes),
  `Item_IconKind` (bytes). `BattleResultWin_DrawFrame`'s colour arguments
  are the clearest case: the red's upper bytes are `y`'s stack slot, the
  green's second byte `h`'s. A fold into `kEngineStandard` would serve every
  window group.
- **`Equip_PreviewSet`'s out-buffers** (the standard listing masks them 0
  and leaves them unfilled - "the group's"): `PreviewEffect` fills the four
  marks with 0..5 (the caller compares them with 4 and 1) and the four
  values, and notes both.
- **Regions** beyond the engine frame: the sprintf buffer `0x904BA0`
  (0x20), the level-up window's eight buffers `0x904D20..0x904E10`, and the
  lists' scroll marks `0x939840` (0x20).

## 5. The fuzz and its seeds

Seeds (`Seed`, by the clone's address): the level-up record's member 0..7
and level 0..98 (the level table's rows); the drops' count 0..8 with words
and counts of 0; the grid's step 4 (and 3, 5), row 0xFF or a cell, column
0..4, the cost 0..0xFF and the menu actor's member byte 0..2 with its AP at
the cost, one above, one below, 0x80 above; each slide's `+4` / `+6` at its
bound and 1, 2, 0x10, 0x20 either side (`0x29`, `-0x17`, `0x5B`, `0x11`,
`-0xA5`, `0x15B`, `0xA3`, `0x143`); the gene rows' indices (0xFF sometimes),
form bytes (0xFF, 0..31, any), the top row inside the list (0..3 of six,
0..9 of twelve), the cursor row 0..2, step 6 / sub-step 5 and their
neighbours; the equipment window's record (a window record) with rows
`+0xA` / `+0xB` 0..6 and BATE's member 0..2 or 0xFF. Words (`Args`):
coordinates a word with garbage above half the time, counts and indices
inside their tables, the frame's cells below 64 (a 255 by 255 grid overran
the harness's 32,768-entry log on the first run - the one start-up Fatal).
The disturbance (`Disturb`, from the hash only) moves what the windows read
again after a call: the drops' count, the window colour, the choice, the
cost, the hand's row and column, a byte of the gene rows, BATE's member.

    COUNTS_LINE

## 6. Controls

`scratchpad/be7/controls.py` (the session's scratch): each control plants
one change in `src/game/battle_e7.cpp` (anchored on a string found once),
rebuilds, runs `BOF3X_SHADOW=battle_e7` headless, restores and rebuilds.

CONTROLS_TABLE

## 7. Latent defects (Capcom's, kept)

- **The four stack dispatchers are unbounded** (`0x598DC0`, `0x598F60`,
  `0x599360`, `0x599470`): a record `+3` past the three (five) entries
  calls whatever dword the stack holds above the table. Within the group
  only the open states move `+3` (by one, to the first slide); what else
  writes it (the gene command's input code) is not read here. Ours aborts
  with a message.
- **The open states write through an unchecked task slot**
  (`0x598FA0`, `0x599390`, `0x5994A0`): `BattleTask_Create`'s answer is used
  as an index with no test, so a full pool (0xFF) writes `+9` / `+0xA` of
  slot 255, 0x83FF bytes past the pool - the shape of D163. Ours aborts.
- **The lists' rows are indexed by the record's signed `+0x12` with no
  bound** (`GeneWin_DrawList` / `_DrawList2`): the reads go wherever `+0x12`
  points; the scroll marks copy a fixed six / twelve rows whatever `+0x14`
  says. Reads only.
- **`BattleResultWin_DrawDrops` places each drop by its index** and skips a
  word of 0 there: the empty first slot of D47 (known-defects, group CD's
  finding) is this function's. Unchanged.
- **Quirks, not defects**: `BattleResultWin_DrawLevelUp` calls `0x432170`
  twice per stat shown; `BattleResultWin_DrawItem` leaves `dim` on the
  stack from its `Item_IconKind` call as `Menu_DrawIcon8`'s fourth word;
  `GeneWin_DrawListFrame`'s third and fourth words are never read;
  `BattleEquipWin_Draw` draws every usable item twice (dimmed, then in its
  colour).

## 8. What nothing reached

Everything each function can reach from its words and seeds was entered
(the coverage line above: every recorder the originals call, every
dispatcher entry about a third of the rounds, `Window_FreeCurrent` from all
three slide-outs). `GeneWin_DrawCursorBox`'s `flash` is 0 from its one
caller; its pulse is reached only through its own words.

## 9. Calls across groups, inbound calls, the rebinding, the live route

**Raw calls out** (`battle_e7_callees.h`): `0x432170` (BE1's, not merged;
`--edges` lists its twelve call sites in `0x597FC0`) and `0x59DB70` (in no
group, catalogue part 7). Every other callee is ours by name or the
standard set's.

**Inbound calls from outside the group:**

| Caller | Owner | Into |
|---|---|---|
| `Window_Handler4Kinds` `0x597F60` stack table (`window_kinds_callees.h` `kResultKinds[1]`) | ours, CM | `0x5984B0` |
| `0x597FA0` stack table | Capcom's, no group | `0x597FC0` |
| `BattleResultWin_DrawExp` `0x5985A0` (`battle_result_callees.h` `kDrawFrame`) | ours, CD | `0x5982D0` |
| `0x598890` stack table | Capcom's, no group | `0x598DC0`, `0x598F60`, `0x599360`, `0x599470` |
| `0x5989B0`, `0x5989F0` | Capcom's, no group | `0x598A30` |
| `0x59CC10` (`Window_Handler8Kinds` kind 3) | Capcom's, no group | `0x59D640` |

**Rebound** (the round-ten form: `bof3::addr::<Name>`, the same value, so
the fuzz keys stand): `battle_result_callees.h` `kDrawFrame` ->
`BattleResultWin_DrawFrame`; `window_kinds_callees.h` `kResultKinds[1]` ->
`BattleResultWin_DrawDrops`. **Left raw**: the comments in
`battle_result.cpp` / `battle_result_fuzz.cpp` (prose), `0x597FA0` in
`kResultKinds[0]` (not ours), and `boss_harness_eh.cpp`'s `0x598DC0` /
`0x598DF0` / `0x598E10` / `0x598E50` rows - the harness's own self-test
copies Capcom's bytes and keys its routes on those addresses (and the brief
forbids editing a harness), so they stay as they are; `boss_harness.h`'s
band comment names `0x597FC0` / `0x59D640` as the band's ends. No raw
reference sits in a file another group of this wave is writing.

**The live route** (`tools/recipes/dragonTransform.txt`,
[`takeover-queue-field-battle.md`](takeover-queue-field-battle.md) section
5): it enters `GeneWin_DrawGrid`, `GeneWin_DrawFrame`, `GeneWin_DrawChoices`
(frame 977, the gene screen), `BattleResultWin_DrawFrame` and
`BattleResultWin_DrawItem` (frames 4,100..4,188, the results) - five of the
31. `0x598810` (the result's EXP-to-next, entered too) is in no group and not
counted. `BattleResultWin_DrawDrops` is `_DrawItem`'s only caller, so it
ran on that route as well, though the trace's list does not name it; the
coordinator's live check after the wave settles it. The other 25 are fuzz
only.

## 10. For `analysis/calltrace/entries_logic.txt`

Appended (main checkout, 2026-09-29): the 25 extents not already there, each
to its last `ret`; the host lines `005982D0 2A0`, `00598BE0 2A7`,
`00598E90 24F`, `005990E0 482`, `00599A00 150` are fixed by the functions'
own smaller extents (`1D8`, `1DE`, `CC`, `274`, `148`). `00598750 B3`,
`00598A30 1A2`, `00599570 201`, `00599780 190`, `00599910 E4`,
`0059D640 521` were already exact.
