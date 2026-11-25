# State by name, group A4: tier 0 - a refused quicksave loads as its save, and the came-from place in a save's tail

**Status:** IN PROGRESS (2026-11-25, the laptop, the Linux x86-64 build of
`sbn/A4`). The state-by-name round's group A4
([`state-by-name-round.md`](state-by-name-round.md) 3.5, 4, 4a; the owner's
calls 2 and 3 and the decision of 2026-11-24 on the save's tail). Built:
`src/game/menu/quick_menu.cpp`, the title's load path in
`src/game/core/sound.cpp` (`TitleFlow_Begin`, `LoadMenu_Read`),
`src/game/save_tail.h` and `src/game/core/save_tail.cpp` (new), the tail's
writers (`FieldSave_Write`, `Save_QuickWrite` in
`src/game/shop/field_save.cpp`; `GameSaveHalf` in `src/hook/snapshot.cpp`),
the save editor (`src/launcher/save/block.{h,cpp}`,
`src/launcher/ui/page_saves.cpp`), `tools/quicksave_check.py tier0`. Not
played by the owner. Ledger: drafts B, C and D (section 7), numbered at the
merge.

## 1. Reads first: what section 9 of the round doc left unchecked

### 1.1 The save's write path

Read from the source, 2026-11-25:

- `Save_BuildBlock` (`0x5806F0`, ours in `src/game/shop/field_save.cpp`)
  gathers the live block `0x9039E0`, copies its `0x10B0` bytes to
  `Save_Staging` (`0x92A0E0`) while summing them, then **clears the next
  `0xD50` bytes** (`rest_2c::at::kStagingClear`, `0x92B190..0x92BEE0`), and
  stores the sum's low word at `+0x70` of the copy. So after a build the
  `0x200` bytes after the block in the staging buffer are zero, whatever they
  held, and nothing else writes them before the file is written.
- `Save_WriteFile` (`0x454870`, `src/game/field/save.cpp`) writes
  `Save_Staging`'s first `size` bytes as they are (`File_Write(handle,
  Save_Staging, size)`); its three callers pass `0x12B0`
  (`rest_2c::at::kSaveSize`): `FieldSave_Write` (the save menu's write, a save
  point or an inn), `Save_QuickWrite` (F12) and nothing else. The quicksave's
  save half is `GameSaveHalf`'s copy of the same staging buffer after the same
  build (`snapshot.cpp`).
- So **the tail is written from the staging buffer as it is, and
  `Save_BuildBlock` zeroes it**: a tail of ours goes in between the build and
  the write (or the copy), and nowhere else. That is what the three callers do
  now (section 3.2). `Save_BuildBlock` itself is not changed: its fuzz compares
  Capcom's (the save callers' fuzz too), and the callers' write sits behind a
  switch armed after every self-test.

### 1.2 The tail is outside the checksum and read by nothing

- `LoadMenu_Read` (`0x5883C0`, `sound.cpp`) zeroes `0x1C00` bytes of
  `Save_Staging`, reads `0x12B0` bytes of the file into it, and sums and copies
  `0x10B0` (`save_menu::at::kBlockBytes`) to the game block. The tail is read
  into the buffer and neither summed nor copied.
- `Save_Staging`'s other readers: `Save_ReadSummaries` reads `0x1C` bytes at
  file offset `0xCA0` into the buffer's start (`Save_ReadFile(name, 0x1C,
  0xCA0)`), nothing after the block. `symbols.toml`'s evidence for
  `Save_Staging` (`pe_xref.py`): five references in `BOF3.exe`, the read, the
  write, `Save_BuildBlock` and two in the save menu; no symbol of
  `symbols.toml` lies in `0x92A0E0..0x92BEE0` but the buffer itself. A read
  through a computed pointer would not be in that count; none is known.
- Measured: **all 76 recipe saves** in `tools/recipe_saves/` on this machine
  (the owner's, every one an ordinary save or an F12 save of the PC port) have
  a tail of `0x200` zero bytes, as the editor spec's 67 of 67 had.
- The PlayStation side: a memory card's save body holds the block at `0x200`
  and the editor's card writer writes `0x200..0x12AF` only
  ([`save-editor-spec.md`](save-editor-spec.md) 5.2, "Replacing a block"); a
  card holds no PC tail, so a converted save has none (zero), and a PC save
  converted to a card leaves its tail behind.

### 1.3 What a return through the edge needs

The two exits that take the party back where it came from, read from the
source:

- **An encounter field's edge** (`Field_LeaderStepLands`, ours in
  `src/game/field/field_4.cpp`): with `Field_InputFlags` bit 1, a step whose
  cell is the map's edge calls `Field_ChangeArea(AreaW2c_LastArea,
  [0x7E091C], [0x7E0920], 4)` and ends the walk. DIV-0121 holds there when the
  word is `0xFFFF`.
- **A mode-8 area's leave** (`GameMode8_Leave` `0x4964E0`, ours in
  `transition.cpp`): the same call with the same three words, after a fade.
  An area whose `Field_InputFlags` has bit 3 enters mode 8 at once
  (`GameMode_Enter`); a game loaded in one has the same `0xFFFF`. Not in
  DIV-0121 and not measured here (no route loads in a mode-8 area); the tail
  serves it too.

`Field_ChangeArea` takes nothing else from the place the party left: it
stores the area, the two positions and the flags, classifies the area and
picks its music; the entry (`Area_Enter`, `GameMode_Enter`) takes it from
there. So the three words are what a return needs **by the read**; that the
walk out lands where it should is section 5's measurement.

`Area_Enter` (`0x594E60`, ours in `src/game/menu/item_trade.cpp`) writes the
three words on every entry: `AreaW2c_LastArea` the area number before the
entry (`Game_AreaNumber`, which `GameMode_Start` has made `0xFFFF` for a
loaded game), the two dwords the leader's `Field_Kind2X` / `Field_Kind2Z` at
that moment. It runs in `GameMode_Enter` (mode 1), not `GameMode_Start` (mode
0). Other readers of `AreaW2c_LastArea`: fifteen operands in Capcom's code, an
area's init or handler comparing it with an area number (`symbols.toml`'s
evidence: `0x40FC40`, `0x413550`, `0x416400`, and twelve more). An init runs
inside `Area_Enter`, before a put-back, so it sees `0xFFFF` after a load as it
always did; a handler that runs later in the area sees the place the save
holds, as it would have in the game the save was made in.

### 1.4 The road for the save half, as read before it was tried

- The restart: F9's second press sets `Game_Paused` 0 and `Game_RestartFlag`
  1 (`magic_080.cpp` `WinGame_Action`); the loop, at the top of its next pass,
  clears the flag, calls `Task_SetStackBase` and creates task 0 at `Boot_Task`
  again. `Boot_Task` clears VRAM's shadow, loads `0x225` (`FIRST.DAT`), zeroes
  the game state `0x9039E0..0x904A90`, resets the windows, makes task 1
  `Title_LoadTask` and ends. Task 1 loads `0x226` and enters `Title_Task`.
- **The title needs a press to reach the menu.** `Title_Task` sets its state
  word `Title_State` (task 1's private word `+0x18`, `0x66C808`) to 0 and
  sleeps; its states load the backdrop, hold 360 frames, show the logo 900
  frames and then run the demo. Only Start (`Title_CheckStart`) leads to
  state 7, `Title_StateMenu`, which stops the music, makes task 0
  `TitleTask_Run` (the title's menu task) and ends task 1. The round doc's road
  ("the restart, then the title's load ... entered at `LoadMenu_Read`") says
  nothing of that press: something must take the title to state 7.
- `TitleTask_Run` runs mode 0 (`TitleMode_Load`, `0x31D`) and then mode 1,
  `TitleFlow_Step` by `Game_Step`: step 0 `TitleFlow_Begin` (the field made
  ready for the title's backdrop, `Save_ListFiles`, step 1), step 1 the title
  menu, step 3 the load menu by its state: 0 the list, 1 open, 2 choose, 3 the
  Yes / No, **4 `LoadMenu_Read`**, 6 `LoadMenu_Loaded`, 7 `LoadMenu_Apply`,
  then step 5 `TitleFlow_EnterGame` (`Task_Restart` of task 0 at
  `Field_Task`). All of it in task 0, with `Task_Sleep` waits, as the round
  doc has it.

## 2. Part 1: the question and the save's load (draft B)

(being written)

## 3. Part 2: the came-from place in the save file's tail (draft C)

### 3.1 The tail's layout

In the file at `0x10B0` (and in `Save_Staging` at `0x92B190`), `0x200`
bytes, little-endian:

| Offset | Bytes | |
|---|---|---|
| `+0x00` | 8 | magic `BOF3XTL\0` |
| `+0x08` | 2 | version, 1: the grammar of this table |
| `+0x0A` | 2 | the records' bytes that follow, at most `0x1F0` |
| `+0x0C` | 4 | FNV-1a of those bytes |
| `+0x10` | | records, one after another: u16 kind, u16 bytes, then that many bytes |
| the rest | | zero |

Kind 1, **the came-from place**, 12 bytes: u16 area (`0xFFFF`: none), u16 0,
s32 x, s32 z (16.16, as `0x7E091C`, `0x7E0920` hold them).

How a later use coexists: it adds a record of its own kind; a reader skips a
kind it does not know (the self-test plants one before the place). The version
says the table's grammar: a reader refuses another version whole (logged, the
save loads as one without the tag), so a change of the head or of the record
form raises it. The FNV makes a tail that is not ours (a hand-edited file, a
tool that writes something there) read as damaged rather than as a place.

(being written)

## 4. Part 3: what a save half taken off a save point lands in

(being measured)

## 5. Measured

(being measured)

## 6. Findings and premises

(being written)

## 7. Ledger drafts

(being written)

## 8. Not checked

(being written)
