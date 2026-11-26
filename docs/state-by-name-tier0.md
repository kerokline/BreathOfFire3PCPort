# State by name, group A4: tier 0 - a refused quicksave loads as its save, and the came-from place in a save's tail

**Status:** STABLE (2026-11-25, verified the same day on the laptop, the
Linux x86-64 build of `sbn/A4`; section 5 has the measurements). The state-by-name round's group A4
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
  Save_Staging, size)`); its two callers pass `0x12B0`
  (`rest_2c::at::kSaveSize`): `FieldSave_Write` (the save menu's write, a save
  point or an inn) and `Save_QuickWrite` (F12). The quicksave's
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
  DIV-0121. The fishing spot (area 30) is one: measured in part 3 (4, 5.3),
  where a game loaded there without the tag crashes on quitting the fishing
  and one with it leaves to the lake's map.

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

### 2.1 What the player sees

- **F5's menu** (`quick_menu.cpp`). A load the core refuses with its save half
  whole (`Snapshot_LastLoad()` answers `RefusedSaveOffered`, A1's
  `Parse`) asks a second question on the refused slot: the title box holds the
  game's own Yes / No alone (system message `0xF` from `FIRST.DAT`'s pool,
  with the hand on Yes, as `Menu_YesNo` starts it), the slots unlit, and below
  them two lines of the 8 x 8 font: the reason, `REFUSED - ANOTHER BUILD (NOT
  PORTABLE)`, and the question, `LOAD THE SAVE IN THIS QUICKSAVE?`. Left and
  right flip the answer; cancel, or confirm on No, goes back to the slots with
  the reason shown, the game as it was. Confirm on Yes acts at its release.
  A load refused with no whole save half is today's refusal, unchanged. No
  slot is marked before a load is tried (the owner's call 3).
- **F7** asks nothing. Its refusal line (`Overlay_Text`, which reaches the log
  while the back buffer is ours, DIV-0145) becomes `Quickload refused:
  another build (not portable) - use F5 to load the save in it` when the save
  half is whole: the core's line copied into a buffer of ours with the
  addition, right after the frame point, so `snapshot.cpp` is not touched.
- **The text.** The question and the addition to F7's line are ours, in
  Simplified Technical English. *There is no language mechanism for text of
  ours inside the game*: the menu's game words are system messages that the
  title's pool holds per language, and the menu's one line of ours, the
  refusal's reason, has always been English in the 8 x 8 font under every
  language (QM.7). The question is drawn the same way, under every language;
  the Yes / No beside it is the game's and follows the language. A mechanism
  for our in-game text per language would be a change of its own (a
  language layer chunk of ours); listed for the owner (section 9).

### 2.2 The road, as built

Yes in the question:

1. **The save half read**: `Snapshot_ReadSaveHalf(slot)`, A1's, into a
   buffer of ours (`0x12B0` bytes). A file that no longer gives one (changed
   since the refusal) keeps the menu open with `NO SAVE - the reason`.
2. **The restart F9's second press asks**: `Game_Paused` 0 and
   `Game_RestartFlag` 1, the menu closed, the sound left paused as that press
   leaves it (after F9's restart the title starts its music afresh; on this
   road the loaded area does, `Area_Enter`'s `Music_Play`). Before it, the title task's
   state word `Title_State` is given a mark of ours, `0xFFFF` (task 1's
   private word `+0x18`: `Task_Create` keeps a record's private words and
   `Task_ClearPrivate` clears only `+8..+0x17`, so nothing but `Title_Task`'s
   entry writes it before the title begins). At the loop's next pass the
   restart runs as F9's does: `Task_SetStackBase`, task 0 created at
   `Boot_Task`, which makes task 1 the title.
3. **The title's menu, without the press** (the premise that did not hold,
   1.4): at the first frame point where `Title_State` is no longer the mark,
   `Title_Task` has begun (it sets the word to 0 and sleeps before its state
   0 runs). The word is set to 7 and `Title_StartArmed` to 0, as
   `Title_CheckStart` leaves them when Start leads to the menu. The next
   frame, the game's own `Title_StateMenu` stops the music, makes task 0
   `TitleTask_Run` and ends task 1. No transition is started: the load
   menu's read follows at once and `LoadMenu_Loaded` starts the fade into the
   game. The title's state 0 (the backdrop's file `0x136`, its music) never
   runs; nothing after it reads either.
4. **The load menu's read, of this file** (`sound.cpp`): `TitleTask_Run`
   loads `0x31D` and runs `TitleFlow_Begin`, whose end, on this road only,
   goes on to the load menu's state 4: the slot summaries all empty but slot
   0, which is the save half's own summary (its `0x1C` bytes at `0xCA0`),
   the slot and the first shown 0, the title cursor on LOAD GAME (row 1;
   `TitleFlow_EnterGame` sends row 2 back to the menu), `Game_Step` 3.
   `LoadMenu_Read` then takes the save half's `0x12B0` bytes into
   `Save_Staging` in place of the file's and does the rest as for any slot:
   the sum, the copy, `Field_ScriptFlags`, state 6. Its log line names the
   quicksave, not "loaded slot 0", which `input_run.py` reads as the moment
   to hand slot 0 back.
5. **Unchanged from there**: `LoadMenu_Loaded` (the windows, the slots with
   the save half's summary, message `0xB1`, the fade, 30 frames),
   `LoadMenu_Apply` (the pad map, the party, `Scenario_Load`) and
   `TitleFlow_EnterGame` (each character's growth roll and stats, task 0
   restarted at `Field_Task`). The game's code applies the save; ours chose
   the file and skipped the menu.

Every piece of ours on this road is a static outside the span (the request's
phase, the buffer, the slot), so the restart, which rewrites the game's state,
does not touch it. In the hosted fuzz (`save_menu_fuzz.cpp`: `TitleFlow_Begin`,
`LoadMenu_Read`, `LoadMenu_Apply`) the road is never armed and both functions
take the original's path.

### 2.3 The first measurement: the road works as built

`tools/quicksave_check.py tier0` (section 5.1) on `combat` with the take at
recipe frame 420, in area 29 (mode 2). The load run's log, in order:
`F7 BISLPS10.DAT refused: another build (not portable) ... (the save half is
whole: it can be loaded as a save)`; `its save half is whole; asked whether
to load it`; `the restart asked (Game_RestartFlag)` at `Frame_Counter` 241;
`the title's task began at Frame_Counter 243 (its state 0): state 7`; `the
title's menu began at Frame_Counter 246: to the load menu's read`; `loaded the
save half of BISLPS10.DAT`; the game in mode 2 at area 29. Five frames from
the restart to the read. The road of the round doc (the restart, then
`LoadMenu_Read` with the save half, then `LoadMenu_Apply` and
`TitleFlow_EnterGame` unchanged) **works, with the one addition of step 3**.

A first trial took the quicksave at recipe frame 320 of `combat`, which is
still the title's load menu (the route's title presses take about 370
frames): its save half is the title's game block, and loaded as a save it
came up in area 0 with every position word 0. Not a crash, and not a game;
part 3 (section 4) has the takes that matter.

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

### 3.2 Who writes it, who reads it

- **Written** by `bof3::SaveTail_Write(tail)` after `Save_BuildBlock` and
  before the write: in `FieldSave_Write` (the save menu: a save point, an inn)
  and `Save_QuickWrite` (F12) into `Save_Staging + 0x10B0`, which
  `Save_WriteFile` then writes as it is; in `GameSaveHalf` into the
  quicksave's copy of the staging buffer (the take puts the span back
  afterwards, so the staging buffer the game sees is untouched there). The
  three words are the live ones: `AreaW2c_LastArea`, `[0x7E091C]`,
  `[0x7E0920]`. A save made where the party came from nowhere writes area
  `0xFFFF`, which a load ignores.
- **Read** by `bof3::SaveTail_Loaded(staging, area)` in `LoadMenu_Read` once
  the block has summed (any slot, and the save half of tier 0's road): no
  magic, a fault, no came-from record or area `0xFFFF` - nothing armed, a log
  line, the load as before; otherwise the put-back is armed with the three
  words and the block's area word (`+0x24`, the area the save resumes in).
- **Put back** by `bof3::SaveTail_Frame()` at the loop's frame point
  (called from `QuickMenu_AfterPoint`, after `Snapshot_Pending`): it waits
  while `Game_Mode` is 0 or 1 (the loaded game's `GameMode_Start` and
  `GameMode_Enter`, whose `Area_Enter` writes the three words, and the title's
  menu while the load is applied); at the first point past them it puts the
  three words back when the party is still in the save's area and
  `AreaW2c_LastArea` is still `0xFFFF`, and drops the request otherwise (an
  entry event took the party on: the word is then a real one). Either way
  once, with a log line. `TitleFlow_Begin` drops a request still waiting (a
  restart before the loaded game entered its area). The condition is a pure
  function (`save_tail::Decide`), self-tested.
- **The switch**: `SaveCameFrom`, a `PatchBytes` of a flag of ours set by
  `SaveTail_Inject` from `QuickMenu_Inject`, after every module's self-test.
  Off: no save writes the tail, no load reads it.

### 3.3 The save editor

`src/launcher/save/block.{h,cpp}` ([`save-editor.md`](save-editor.md) 1)
read the block and wrote the tail as zero. Now `ReadPcFile` and `WritePcFile`
have forms with a `Tail` (`0x200` bytes), and the page
(`src/launcher/ui/page_saves.cpp`) keeps the opened PC file's tail and writes
it with the block (`save::KeptTail`), unless the edit changed the area the save
resumes in (`+0x24`): the came-from place is the way out of that area, so a
save moved to another area is written with a zero tail and holds at an edge as
any untagged save. A card's save has no PC tail, and a save written to a card
leaves it behind. The host test (`tools/launcher_host/save_tests.cpp`
`TestFile`) writes a file with a tail and reads both back, and checks the two
rules of `KeptTail` and the zero tail of the old form.

## 4. Part 3: what a save half taken off a save point lands in

A measurement, not a list to fix. Each take was loaded once as its save, by
the road of part 1 (F5, the refusal, Yes) and, for comparison, as slot 0
through the title's own menu; the two came up alike in every case (section
5.1's table). The save half is `Save_BuildBlock`'s block, the field's state as
F12 saves it, so what a take keeps is what an F12 save at that frame keeps.

- **In a battle** (`combat`, recipe frame 900, mode 5, area 29: Rei's Pilfer
  mid-turn): the game comes up **in the field of area 29**, where the party
  stood before the encounter (cell `0x1A`, `0x19`), the battle gone - as the
  owner found for F12 in a battle (IDEAS I18). The game goes on: the field
  answers the pad (5.3). The came-from place is `0xFFFF`: the `combat` save
  was loaded in area 29 and the party had not left it, so this save too holds
  at the edge (draft D).
- **In a scene** (`cutsceneAndNue`, 2165, mode 4, area 23: the night in the
  house, a line typing out): the game comes up **in area 23, in the house**,
  the party at (`0x47`, `0x5`), no scene running and no window open (the save
  half holds no script state). The came-from place is area 23 itself at
  (`0x180000`, `0x178000`): the scene had moved the party within the area
  (an area change to the same area), and the tail carries that and puts it
  back. Whether the story continues from there as it should (the scene's
  flags as the block holds them mid-scene) is the owner's to judge; nothing
  stuck or crashed in the frames run (5.3).
- **In a minigame** (`caughFish`, 2050, mode 8, area 30: the fishing spot):
  the game comes up **at the fishing spot, area 30, in mode 8**, the fishing
  started afresh ("Set rod & lure ... Casting"), the catch in progress gone.
  The fishing spot is a mode-8 area, so this is the second exit of 1.3 met in
  a real route: its came-from place is area 16 at (`0x300000`, `0x240000`)
  (the lake's map, which the party left by Speak at the spot), put back at
  `Frame_Counter` 283. **Quitting the fishing** (cross, the prompt "Do you
  want to quit?", down to EXIT, circle) takes `GameMode8_Leave` with those
  words (5.3).
- **A crash found, for the owner** (5.3): the same save half **without the
  tag** - which is what every F12 save made at the fishing spot before this
  change is, and every quicksave's save half taken there - loaded, then
  quit: `GameMode8_Leave` calls `Field_ChangeArea(0xFFFF, ...)` and the game
  stops with SIGSEGV reading address 0 in `Area_ZoneAtIn` (`bof3x+0x11E4C4`,
  under `Area_PickMusic`, `Field_ChangeArea`, `GameMode8_Leave`,
  `Field_Task`): known-defects D270's null zone list, reached by the mode-8
  leave, which DIV-0121's hold does not guard. Draft C removes it for every
  save made from now on (the tag); an untagged save still crashes there.
  Proposed for `known-defects.md` (not numbered here):

  > **Quitting the fishing in a game loaded at the fishing spot stops the
  > game.** An F12 save (or a quicksave's save half) made at the fishing spot
  > (area 30, mode 8) loads there with the came-from area `0xFFFF`
  > (`GameMode_Start`); quitting the fishing (`GameMode8_Leave` `0x4964E0`)
  > asks `Field_ChangeArea(0xFFFF, ...)`, and `Area_ZoneAtIn` reads address 0
  > (D270's path). Measured 2026-11-25 on the Linux ELF of `sbn/A4`
  > (`p3f/leave_0`). A save made by a build with draft C carries the place and
  > leaves to area 16; for an untagged save the fix would be DIV-0121's hold
  > applied in `GameMode8_Leave` too (the fishing then has no way out but a
  > reload), or a default place per mode-8 area. The owner's call.

## 5. Measured

The laptop, 2026-11-25, the Linux x86-64 ELF of `sbn/A4`, headless,
`--direct`, `en-US`, the cache and image of the main checkout's `game/`.
Captures outside the tree, in `~/bof3x-runs/sbn1125/A4/root/` on the laptop
(they hold game data: saves, quicksaves, pictures).

### 5.1 Part 1: a doctored quicksave refused, Yes, the game from its save half

`tools/quicksave_check.py tier0 ROUTE --at F` (new). Four steps, each a run
of `input_run.py`, one game at a time:

1. **take**: the route to idle recipe frame F, `key f6` there (the mode and
   area peeked first, no frame of their own), 30 frames, the end:
   `BISLPS10.DAT` in the run's game folder.
2. **doctor**: one byte of the build stamp (header `+0x18`, file `0x12C8`,
   the version tag) flipped, so this build refuses the file as another
   build; its first `0x12B0` bytes written apart as a recipe save,
   `T0HALF.DAT`.
3. **load**: in the take's game folder (`--run-game none`), from the title:
   `key f5` at 150, confirm on slot 0, Yes (the load, refused), a shot of the
   question, Yes (the save); `every 300`; once `Game_Mode` is 2, 120 frames,
   the peeks (the area, the leader's x and z, the came-from words) and a shot.
4. **ref**: `T0HALF.DAT` as slot 0 through the title's own load menu
   (`combat.txt`'s presses), the same wait, peeks and shot.

The proof passes when the load run's log shows each step of the road and the
two runs' peeks and last pictures are equal.

| Route, take | The take | The road in the load's log | Load against ref | Captures |
|---|---|---|---|---|
| `combat` 420 | mode 2, area 29 | refused "another build (not portable)", the question, the restart at 241, the title's task at 243, the read at 246 | equal: area `0x1D`, (`0x1A0000`, `0x190000`), came-from `0xFFFF`; picture equal | `t0a/` |

The question's picture (`t0a/load/t0_question.png`): the title box with the
game's Yes / No and the hand on Yes, the slots, and below them `REFUSED -
ANOTHER BUILD (NOT PORTABLE)` and `LOAD THE SAVE IN THIS QUICKSAVE?`, every
character drawn (the `?` included). F5 was pressed on the title screen, so the
backdrop is the title's own picture tiled, as the menu draws there today.

### 5.2 Part 2: the walk to the edge, with and without the tag

**No recorded route here enters area 29 by play.** Area 29 has no link list
and no link of any area leads to it (the recipe step `links`: 192 areas, 1,542
links, `m/links.log`); it is the world map's encounter field, entered by the
world map's own code. `worldMapAndAreaTransition` (watched, `m/wm.log`) goes
7, 33 (the Yraall Region map), 8, 33, 7, 14 and never into 29. So the saves
were made with the harness's `goto` (`Field_ChangeArea` through the
original's entry, the funnel a save's load and every scripted change use,
[`door-sweep.md`](door-sweep.md)):

- `p2/make29.txt`: the `combat` save (area 29, came from nowhere), then
  `goto 33 0x1F0000 0x1B0000 4` - the world map at the place area 8's link
  8.8 lands - then `goto 29 0x190000 0x190000 4`, the save's own cell. The
  watches show `Area_Enter` record area 33 and (`0x1F0000`, `0x1B0000`).
  Then F12 (`save tail: came from area 0x21 at (0x1F0000, 0x1B0000)`) and F6.
  Both files' tails hold the tag and that place, the rest zero:
  `p2/saves/T29F12.DAT` (the F12 save), `p2/saves/T29QSH.DAT` (the
  quicksave's first `0x12B0` bytes).
- `p2/stallion_cut.txt`: `stallion.txt` cut at 11,500 frames, with watches on
  the area, the came-from words and the mode, at `--speed 8` as DIV-0121's
  verification ran it, from three saves:

| Save | Its tail | What happens |
|---|---|---|
| `combat` | none (`save tail: no tag`) | the party never leaves area 29 in 11,500 frames: DIV-0121's hold, as before |
| `T29F12` (F12) | came from 33 at (`0x1F0000`, `0x1B0000`) | put back at `Frame_Counter` 370 (mode 2, area 29); **at recipe frame 10,745 area 29 -> 33**, the came-from words now area 29 at (`0x68000`, `0x20000`): the leader's cell on the edge (z = 2); at 10,881 the world map's encounter takes the party into area 29 again, recording area 33 at (`0x210000`, `0x1C0000`), two cells from where it had landed |
| `T29QSH` (the quicksave's save half, as slot 0) | the same | the same, frame for frame |

So the party leaves to where it came from, at the edge DIV-0121 holds, and
the world map's own encounter entry into area 29 records exactly what the tail
stands in for: the world-map area and the party's cell on it. The edge's step
lands at recipe frame 10,725 or so; the area word changes 20 frames later
(mode 1 at 10,744), as after the `goto` (413 to 433).

### 5.3 Part 3: takes in a battle, a scene and a minigame, each loaded as its save

`tier0` on three routes (`p3_combat/`, `p3_cutsceneAndNue/`,
`p3_caughFish/`), then a walk or a quit after the load (`walk/`, `p3f/`):

| Route, take | The take | Loaded as its save (F5 road and title alike) | Then |
|---|---|---|---|
| `combat` 900 | mode 5 (a battle), area 29 | area 29, the field, (`0x1A0000`, `0x190000`); came-from `0xFFFF`; picture equal | a 40-frame walk: one cell, and the field's random encounter starts a battle (mode 5) - the game goes on |
| `cutsceneAndNue` 2165 | mode 4 (an event), area 23 | area 23, the house, (`0x470000`, `0x50000`), no scene running; came-from area 23 at (`0x180000`, `0x178000`) put back; picture equal | a walk: to (`0x450000`, `0x80000`), mode 2 - the game goes on |
| `caughFish` 2050 | mode 8 (the fishing), area 30 | area 30, mode 8, the fishing started afresh; came-from area 16 at (`0x300000`, `0x240000`) put back at `Frame_Counter` 283 (337 through the title's menu) | quit (cross, down to EXIT, circle): **with the tag** mode 1 at 818, **area 16 at (`0x300000`, `0x240000`)**, mode 2 at 854 (`p3f/leave_T2`); **without the tag** (the same bytes, the tail zeroed: `p3f/leave_0`) **SIGSEGV** reading 0 in `Area_ZoneAtIn` (`bof3x+0x11E4C4`) under `Area_PickMusic`, `Field_ChangeArea`, `GameMode8_Leave`, `Field_Task`; area `0x1E`, mode 8 step 2 |

The fishing's first `tier0` comparison (`p3_caughFish.out`) waited for mode
2, which a mode-8 area never reaches, so neither run reached its peeks. With
the wait for any mode past 1 (`& 0xE`), on the final build
(`p3_caughFish2.out`): every peek equal - area `0x1E`, the leader at
(`0xF0000`, `0x410000`), the came-from place area 16 at (`0x300000`,
`0x240000`) - and the pictures alike but for the lake's animated water
(208,699 of 307,200 pixels): its phase follows the frame counter, which the two
roads reach at different values (the put-back at `Frame_Counter` 283 by the F5
road, 337 by the title's menu).

### 5.4 No state hash moves on a route that never saves or loads a tagged save

`bridgeWalk` (en-US) and `combat` as state-hash runs under `orig6`'s
conditions ([`state-hash.md`](state-hash.md) 6: `--only-slot0`,
`BOF3X_SCALE=2`, `BOF3X_OPT=none`, `BOF3X_LAYERING=0`, the skip list), this
build against the round base's (`~/bof3x-runs/sbn1125/round/wt`), script
`sh/run.sh`:

| Route | `statehash.py diff` |
|---|---|
| `bridgeWalk` | identical on all 2,074 compared ticks |
| `combat` | identical on all 2,602 compared ticks |

Run three times as the build moved (`sh/` on the first build, `fin/` after
the F7 line's fix, `fin2/` on the final build): identical each time.

Both routes load their recipe save through the title, untagged, so the load
reads the tail, finds no tag and arms nothing; neither saves.

### 5.5 The rest

- **The build**: clean, no warning in the files of this change (`fin2/build.log`).
- **The self-test**: exit 0, 10,406 log lines (`fin2/selftest.log`); the new
  lines: the save tail's self-test, the quick menu's question (No, cancel,
  Yes as `LoadSave` at the release), `patch ON SaveCameFrom`.
- **F7** on the doctored quicksave (`fin/f7.log`): `Quickload refused:
  another build (not portable) - use F5 to load the save in it`.
- **The editor's host test** (`tools/launcher_host`, target `save_tests`,
  built under `root/lh/`): 120 checks, 0 failures, the tail's among them.
- **The no-data checks**: `ledger_check.py` (245 entries, 0 errors),
  `ledger_switches.py --check` (with `code_names.h` regenerated for
  `SaveCameFrom`), `ledger_registry.py --check`, `region_table.py --check`,
  `reloc_bare.py --check`, `tu_renames.py --check`, `tu_census.py --check
  --strict`, `retire_shells.py --check --strict`, `os_census.py --headers`,
  `quicksave_check.py selftest`; `width_census.py`, `data_census.py`,
  `reloc_census.py` and `typed_left.py` regenerated for the new deliberate
  sites (three width sites of `save_tail.cpp`; two raw sites of
  `TitleFlow_ToSaveHalf` listed with their neighbours' reasons) and for the
  drift already in the base (`region_table.cpp`'s `addr_eq`, A1's
  `snapshot.cpp` rows), each `--check` passing after. `data_census.py
  --strict` passes.

## 6. Findings and premises

- **The road needed one step the round doc did not name** (3.5, section 9's
  "read from the source, not tried"). After the restart the title waits for
  Start on the logo before it makes the menu task (`Title_CheckStart`, then
  `Title_StateMenu`). The built road puts the title task's state word to 7
  once `Title_Task` has begun, which is where Start leads; everything else is
  the round doc's road, measured working (2.3, 5.1). Applying the save in
  place at the quicksave point was not needed and not built.
- **There is no language mechanism for in-game text of ours.** The menu's
  text is the game's system messages, copied per language from `FIRST.DAT`'s
  pool; the one line of ours it had (the refusal's reason) was English in the
  8 x 8 font under every language. The question follows the same way (2.1).
- **The put-back's place.** `Area_Enter`, which writes the three words, runs
  in `GameMode_Enter` (mode 1), not in `GameMode_Start` (mode 0): the
  put-back waits for the first frame point past mode 1, so an area's init
  still sees `0xFFFF` after a load (1.3, 3.2). It is called from
  `QuickMenu_AfterPoint` at the loop's frame point, so neither
  `transition.cpp` nor `magic_080.cpp` is touched.
- **The tail serves a second exit.** `GameMode8_Leave` (an area of mode 8)
  takes the party back with the same three words, so a game loaded in such an
  area had the same `0xFFFF`; the put-back covers it (any mode past 1). Not in
  DIV-0121 and not measured: no route here loads in a mode-8 area.
- **Area 29 is entered only by the world map's encounter code** (no link
  leads to it), and the encounter records the world-map area and the party's
  cell there, seen at recipe frame 10,881 of the tagged walk (5.2). No
  recorded route on the laptop enters it by play; the proof's entries are the
  harness's `goto`.
- **A save made where the game came from nowhere carries `0xFFFF`.** The
  `combat` save's own quicksave half, taken before the party left, says so
  (5.1, part 3's battle): such a save still holds at the edge (draft D).
- **Saves already on disk.** Every save made before this change, and every
  save of the original port, has a zero tail and loads as before (76 of 76
  recipe saves measured here).
- **The state hash.** `Save_Staging` is in the span, so from the frame a route
  saves (F12, a save point) or loads a tagged save, its page `0x92B000` (the
  tail is `0x92B190..0x92B38F`) differs from a build without the tail. No
  committed file holds a save file's bytes or a hash reading (the readings
  live outside the tree, on the machines); a reading of a route that saves,
  taken before this change, differs on that page from the save on.

## 7. Ledger drafts (numbered at the merge, 2026-11-25: B into DIV-0147 and DIV-0145, C is DIV-0248, D into DIV-0121; the fishing crash is known-defects D439)

Numbered at the merge. Each in the ledger's entry format; an amendment is the
paragraph to add to the entry it amends, with the fields of that entry as they
stand after it.

### Draft B: amendments of DIV-0147 (the question and the save's load) and DIV-0145 (F7's line)

The neighbours decided their switch already: F5 and F7 are read only in our
window procedure, so `Game_WndProc` (costly) is DIV-0145's and DIV-0147's
switch, and the question is part of F5's menu and the line part of F7's. No
narrower name is added: the question exists only after a refusal, which
exists only in this round. Default on, as the menu is.

**To add to DIV-0147:**

- **Amended 2026-11-25 - a refused quicksave loads as its save** (the
  state-by-name round's group A4, [`state-by-name-tier0.md`](state-by-name-tier0.md)
  2; the owner's calls 2 and 3 of 2026-11-24 and 2026-11-25): when a load is
  refused for its build or process and its save half is whole (DIV-0145 as
  amended by draft A), the menu asks on that slot: the reason and "LOAD THE
  SAVE IN THIS QUICKSAVE?" in the 8 x 8 font below the slots (ours, in
  Simplified Technical English, under every language, as the reason always
  was), the game's own Yes / No with the hand in the title box. No, or cancel,
  goes back to the slots and leaves the game as it was. Yes reads the save
  half and asks the restart F9's second press asks (`Game_RestartFlag`); once
  the title's task has begun, its state is put to 7, `Title_StateMenu`, where
  Start on the logo leads, so the title's own code makes the menu task; the
  menu's first step (`TitleFlow_Begin` `0x587DC0`) goes to the load menu's
  read (`LoadMenu_Read` `0x5883C0`) with the save half as the bytes read in
  place of a `BISLPS0?.DAT` (the slot list showing its summary in slot 0),
  and `LoadMenu_Loaded`, `LoadMenu_Apply` and `TitleFlow_EnterGame` run as
  for any save. No slot is marked before a load is tried. Measured: the game
  comes up where the title's own load of the same save half puts it (the
  area, the leader's position, the came-from place, the picture), on
  `combat` in the field; section 4 has a take in a battle, a scene and the
  fishing minigame.

**To add to DIV-0145:**

- **Amended 2026-11-25 - F7 names the way to the save** (group A4, draft B):
  a refusal whose save half is whole adds to F7's line "- use F5 to load the
  save in it"; F7 asks nothing.

The fields of both entries stay as they are, with:

- **Saved state:** unchanged - the question and the road change code and
  presentation; no byte a quicksave holds is read another way (the road's
  request and buffer are statics of ours, outside the span)

### Draft C: a new entry

### A save keeps where the party came from: an edge leads back after a load

- **ID:** (numbered at the merge)
- **Date:** 2026-11-25
- **Subsystem:** save files and the field: the save's writers
  (`FieldSave_Write` `0x580010` and `Save_QuickWrite` `0x5809C0`, ours in
  `src/game/shop/field_save.cpp`; the quicksave's save half, `GameSaveHalf`
  in `src/hook/snapshot.cpp`), the load (`LoadMenu_Read` `0x5883C0`,
  `src/game/core/sound.cpp`), the put-back at the loop's frame point
  (`src/game/core/save_tail.cpp`); [`state-by-name-tier0.md`](state-by-name-tier0.md) 3
- **Tier:** Sensible - the PC port's own save-anywhere (F12) stands a save
  where the original's saves never stood; the save keeps what the way out needs.
- **Original behaviour:** a save file is `0x12B0` bytes, the `0x10B0`-byte
  block and `0x200` bytes that `Save_BuildBlock` clears and `Save_WriteFile`
  writes as zero (measured: 76 of 76 recipe saves on the laptop; the editor
  spec's 67 of 67); the load neither sums nor copies them. `GameMode_Start`
  makes `Game_AreaNumber` `0xFFFF` before it enters a loaded game's area, so
  `Area_Enter` records the area the party came from as `0xFFFF` and the
  position as the leader's stale cell; an encounter field's edge
  (`Field_LeaderStepLands`) and a mode-8 area's leave (`GameMode8_Leave`
  `0x4964E0`) then lead to area `0xFFFF`: Capcom's crash (known-defects D270),
  ours holds (DIV-0121).
- **New behaviour:** every save - a save point's, F12's, a quicksave's save
  half - writes in those `0x200` bytes a tag (`BOF3XTL`, a version, the
  records' length and FNV-1a) and the came-from place: `AreaW2c_LastArea`
  `0x802290` and the dwords `0x7E091C`, `0x7E0920`. A load of a tagged save
  whose area is not `0xFFFF` puts the three words back at the first frame
  point after the loaded game has entered its area (`Game_Mode` past 1), when
  the party is still in the save's area and the word is still `0xFFFF`; so
  the edge leads where it led when the save was made. An area's init, which
  runs inside `Area_Enter`, still sees `0xFFFF` after a load, as before. A
  save without the tag loads as before. The save editor keeps a PC file's
  tail, and writes it zero for a save it moves to another area.
- **Rationale:** the owner, 2026-11-24: fix DIV-0121's cause in the save, so
  that the save a refused quicksave offers (tier 0, draft B) and every F12
  save are used as they are. The position is needed with the area: the place a
  field is entered from on the world map is not fixed (area 29 is entered by
  the world map's encounter at the party's cell, measured: section 5.2).
- **Also in the PSX version?** No - a memory card's file holds the block
  alone, and the disc saves at save points (whether a PlayStation save can
  stand in an encounter field is not established: DIV-0121).
- **Verification:** the self-test `save tail` (`BOF3X_SELFTEST_ONLY`: the
  tail written and read back, the faults, an unknown record kind skipped, the
  put-back's condition). `stallion.txt` (cut at 11,500 frames, `--speed 8`)
  from an F12 save and from a quicksave's save half, both made in area 29
  after the harness entered it from the world map: the put-back at
  `Frame_Counter` 370, and at recipe frame 10,745 the edge takes the party to
  area 33 at the place it came from; from the `combat` save (no tag) the party
  stays in area 29 (DIV-0121). The fishing spot (area 30, mode 8): a
  quicksave's save half taken there loads with its came-from place, area 16,
  and quitting the fishing leaves to it; the same bytes with the tail zeroed
  crash on the quit (`GameMode8_Leave`, D270's path; a finding for
  `known-defects.md`). [`state-by-name-tier0.md`](state-by-name-tier0.md) 5.2, 5.3.
  **Not seen by the owner.**
- **Reversible?** `BOF3X_ORIGINAL=SaveCameFrom`: no save writes the tail and
  no load reads it, DIV-0121's hold as before. A player turns it off with
  `BOF3X_OFF=SaveCameFrom` (the ini's `off=`).
- **Switch:** SaveCameFrom
- **Default:** on
- **Player:** yes - BOF3X_OFF=SaveCameFrom (ini off=)
- **Toward:** none - no release saves the place the party came from
- **Languages:** any
- **State:** active
- **Saved state:** unchanged - the tail is the save file's, outside the block; in a quicksave's span only `Save_Staging`'s tail holds it after a save, a transient that `Save_BuildBlock` clears before every save and `LoadMenu_Read` before every load and nothing reads; the put-back's request is a static of ours

### Draft D: an amendment of DIV-0121

- **Amended 2026-11-25 - when the hold is still reached** (the state-by-name
  round's group A4, draft C): a save made by this build carries the place the
  party came from, put back after its load, so an edge leads there and the
  hold is not reached. It is still reached after the load of a save without
  the tag (every save made before draft C, a PlayStation save converted, a
  save written with `SaveCameFrom` off, a save the editor moved to another
  area), of a tagged save whose came-from area was itself `0xFFFF` (made in a
  game loaded there from an untagged save before the party left), and when
  the put-back is dropped (an entry event took the party on). Measured:
  `stallion.txt` from the `combat` save still stays in area 29; from a tagged
  save made there it leaves at the edge (draft C).

DIV-0121's fields stay as they are (`EdgeNoArea`, default on, `Player: yes`):
the hold is the floor for an untagged save, and its switch stays its own. With
the amendment:

- **Saved state:** unchanged - the hold reads a word the game already holds

## 8. Not checked

- **The owner's eye**: nothing here is seen by the owner. The question's
  screen is seen on one capture over the title screen only; over the field
  and in a battle (whose backdrop the menu leaves out) it is not captured.
- **The Windows builds** (32-bit and 64-bit) are not built here; the change
  has no width- or system-dependent part that is known, but it is not run.
- **The road from a battle's or a scene's F5**, that is with the quick menu
  opened in mode 5 or 4 and the restart taken from there: every load here
  opened F5 on the title screen. The restart is F9's, which a player takes
  from anywhere; not measured from those modes.
- **Mode-8 areas other than the fishing spot**: the leave is the same
  function; only area 30 was run.
- **The editor's window** (`page_saves.cpp`): the tail's path through the page
  is read, not clicked; the format core's host test ran (5.5).
- **An entry event that takes the party on** before the put-back (the Drop
  path): self-tested as a condition, not met in a run.
- **A quicksave taken between a load's read and its put-back** (a few frames):
  the request is a static of ours, so an exact restore of such a take does not
  put the words back; the take's span holds `0xFFFF` and the edge holds. Not
  run; the window is the frames of `LoadMenu_Loaded` and `LoadMenu_Apply` (30
  and more), where F6 is a player's choice.
- **The music after the road**: the restart leaves the sound paused as F9's
  does and the game's music starts afresh in the field; heard by no one here
  (headless, the dummy audio driver).

## 9. Questions for the owner

1. **The question's text is English under every language** (2.1), as the
   refusal's reason has been. Accept, or want a mechanism for our in-game
   text per language (a language layer's chunk of ours) as its own change?
2. **The question starts on Yes**, as the game's Yes / No does
   (`LoadMenu_Choose` sets the answer to 1). Yes ends the game being played
   for the save. Keep Yes first, or start on No?
3. **Draft C's switch** `SaveCameFrom`, default on, a player's switch
   (`BOF3X_OFF`), as DIV-0121's and DIV-0180's neighbours have it. Agree?
4. **The editor drops the tail of a save it moves to another area** (3.3).
   Agree, or keep the tail always?
5. **The title skip writes the title task's state word** (2.2, step 3), the
   state Start leads to, instead of pressing Start. The alternative would be
   to feed the press itself; the state word is the smaller and the
   measured one.

