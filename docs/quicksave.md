# Quicksaves: the snapshot core (F6, F7, `BISLPS1X.DAT`)

**Status:** IN PROGRESS (2026-10-19, the quicksave round's group SN, branch
`quicksave/sn`; built, self-tested headless in the exe host, measured
in-process once - section 8. Audio and graphics have no participant yet:
a quick-load is visibly and audibly imperfect there until stage B, by
design of the round. Not played by the owner.)

The round's plan is [`quicksave-round.md`](quicksave-round.md); SC's census
of what lives outside the span is [`quicksave-census.md`](quicksave-census.md)
(branch `quicksave/sc`). This document is the format, the point and the
registry as built: `src/hook/snapshot.{h,cpp}`, the two keys and the point
in `src/game/win_main.cpp`.

## 1. Two premises of the round doc that did not hold

Both found by SC's census and re-read here before building on them:

- **The task stacks are not in the image.** `Task_SetStackBase`
  (`task_sched.cpp`) sets `Task_StackTop` (`0x66C858`) to the arriving `esp`
  minus `0x4000` on the **main thread's stack**, and task *k*'s stack is the
  `0x4000` bytes below `Task_StackTop - k * 0x4000` (`Task_Create`). So the
  four suspended tasks' frames are `[Task_StackTop - 0x10000, Task_StackTop)`,
  outside the span. The skip list's row `0x66C854 37292 # ... the task
  stacks` names `Task_SchedulerEsp`, `Task_StackTop` and unread data, not the
  stacks. The snapshot copies the slices as a second region ("the task
  arena", section 3) and refuses a file whose `Task_StackTop` differs.
- **Our code is not at a fixed address in the exe host.** `bof3x.exe` has no
  relocations, but our code is `bof3x.dll`, linked `DYNAMIC_BASE`; SC saw one
  build load `0x510000` apart from two directories. The slices' return
  addresses and the overlay's pointer slots name the DLL where it was, so the
  header records the DLL's base and a load refuses another. Whether to link
  the DLL at a fixed base is the owner's call (the coordinator has put it);
  nothing here changes if it is.

## 2. The point

`WinMain`'s loop (`win_main.cpp`), after `Task_RunAll`, `Gfx_LinkOTags` and
`Frame_Counter += 1`, before the next iteration's latch and draw:
`Snapshot_Pending()`. There every task is suspended on its slice and the main
thread holds only the loop's locals. `Game_WndProc` only **asks**
(`Snapshot_RequestTake`, `Snapshot_RequestLoad(slot)`); a key arrives
mid-frame, so nothing is copied or written from the window procedure. The
quick menu (stage B, QM) asks a load the same way.

**On a stack of its own.** The loop stands about `0x3E00` bytes above slice
0 (census 2.1). The take, the restore, the file layer, the DLL's hash and
every participant's `Save` / `Restore` run on a fiber of the main thread
(1 MB reserved, 256 KB committed; `OnOwnStack`), with the game's x87
control word carried over. A take therefore copies slice 0 as the task left
it, and a restore writes the slices **last**, after the participants, so
nothing writes into them after. Before switching, a load touches the four
slices' pages from the top down on the main thread's own stack (only the
owning thread grows a stack through its guard page).

## 3. What is copied

| Region | Exe host | Hosted (`BOF3.exe` + `bof3x.dll`) |
|---|---|---|
| the span | `.data`, `.bss`, the end of `.game` and `.pool`: `0x5DA000..0x987000`, `0x3AD000` bytes (`exe_layout.h`: `kDataVa`..`kOwnVa`) | `.data` and `.bss`: `0x5DA000..0x93E000` |
| the pool | inside the span (`.pool` `0x93F000`) | DIV-0062's array where `DrawPool_Reserve` put it (`draw_pool::Items()`, `0x48000` bytes); none when the grow fell back to Capcom's array (then it is in the span) |
| the task arena | `[Task_StackTop - n, Task_StackTop)`, `n` the committed part of the four slices from the top, at most `0x10000` | the same |
| participants | a record each (section 5) | the same |

**`.rdata` (`0x5C4000..0x5DA000`) is not copied** (census 5): the exe host's
start-up writes it whole (constants, the bound import slots, one pointer
into our DLL) and no game code is known to write it afterwards. The take
records its FNV-1a and a load logs `.rdata unchanged` or, loudly, `CHANGED`
(section 8 has the measurement).

**The keep list.** Bytes inside the span that belong to the process, read
live before the span is written and put back after it (`kKept`, each
checked against `symbols.toml` at start-up):

| Range | Why |
|---|---|
| `Task_SchedulerEsp` `0x66C854` | `Task_RunAll`'s own `esp`, rewritten at its every entry |
| `Game_Hwnd` `0x6BC620`, `Game_HInstance` `0x6BC624` | the window and the module of this process |
| `Frame_Deadline` `0x6BC628` | the original loop's float, unwritten by ours (DIV-0047); the loop's deadline is its own local |
| `Fps_Shown` `0x6BC634`, `App_Active` `0x6BC63B`, `Cfg_Fullscreen` `0x65DA44`, `Cfg_RenderMode` `0x65DA48` | the player's and the process's settings (F8's mode survives a load) |
| `Key_Table` `0x7DE7A8` (128) | the launcher's key table |
| `DInput_Object` .. `Gpu_DispEnv` `0x7DE928` (952) | the input objects |
| `Font_GlyphData` `0x7CC35C` | a heap block made once a launch |
| `Gfx_WindowRect` .. `Dd_CellBackdrop` `0x6BE1D0` (2,096), `Gfx_ClutRows` `0x6C2A40` (4,096), `D3d_DeviceDesc` `0x7CC238` (252), `Dd_DirectDraw` `0x7CC334`, `Dd_StageSurface` `0x7CC344`, `D3d_Device` `0x7CC350` | the display's window rectangle, mode list, row pointers and objects; what the caches and rows **hold** is SP-G's to clear and reconvert |

`Overlay_Text` / `Overlay_Frames` are in the span: the "Quickload" line is
written after it.

**Guards.** A take with any of the sixteen `File_Slots` (`0x7DE3E4`) open is
refused (none is open between frames; a handle would be another process's).
F6 and F7 are refused while `BOF3X_GAPWATCH` is set (its guard pages and
bookkeeping are over the span).

## 4. The file `BISLPS1X.DAT`

In the game directory, beside the saves; `X` = `0`..`F`. Little-endian.

| Offset | Bytes | Field |
|---|---|---|
| `0` | `0x12B0` | **the save half**: `Save_BuildBlock`'s staging copy, built as F12's `Save_QuickWrite` builds it (the slot word `0x9036D4` set to 0 first); the summary at `0xCA0` where `Save_ReadSummaries` reads its `0x1C` bytes. Building it writes the live block's checksum word, the staging buffer and a summary entry; the take writes the span back over them, so the game never sees the build |
| `0x12B0` | 8 | magic `BOF3XQS\0` |
| | 4 | format version (1) |
| | 4 | header size (160) |
| | 4 | file size |
| | 4 | host: 0 `BOF3.exe`, 1 `bof3x.exe` |
| | 32 | the version tag (`BOF3X_VERSION_TAG`, the launcher's version block) |
| | 64 | `bof3x.dll`'s SHA-256, hex (the file, as [`releases/README.md`](releases/README.md) hashes) |
| | 4 | `bof3x.dll`'s load address |
| | 4 | `Frame_Counter` at the take |
| | 8 | span base, length |
| | 8 | pool base, length (0, 0 in the exe host) |
| | 8 | `Task_StackTop`, arena length |
| | 4 | `.rdata`'s FNV-1a at the take |
| | 4 | participant count |
| `0x1350` | | the span, the pool, the arena, in that order |
| | | per participant: tag (16 bytes, NUL-padded), length (4), the record |

**The build id** is the tag, the DLL's hash, the host and the DLL's base. The
hash of the 64 MB DLL is taken once, at the first F6 or F7, on the own
stack.

**A load checks everything before writing a byte**, and a refusal names why
in the overlay and the log, the game untouched: not a save file (under
`0x12B0` bytes); no snapshot (no header); another format; truncated (the
header's size, or a record past the end); another host; **another build**
(the tag or the hash); another process (the DLL's base, or `Task_StackTop`);
another span; **a record whose tag has no participant**; **a participant with
no record** (rule 4: never a partial restore); trailing bytes. The save half
does not depend on any of it: a file refused for another build still has a
save file's first `0x12B0` bytes, summary included, so the quick menu can
draw it and a later "load as a plain save" needs no format change.

**Trust.** A quicksave is this native process's own memory - the span, the
task stacks with their return addresses into `bof3x.dll`, our statics -
written straight back on a load, not an emulator's save state interpreted by
a machine model. A doctored quicksave can therefore run anything. The build
and DLL-base checks do not stop that: they catch an honest file from another
build or process, not a hostile one. Treat a quicksave like a program: load
your own, never one from someone else. Nothing more is built for it this
round; [`IDEAS.md`](IDEAS.md) I46 holds the follow-up.

**The restore**, once checked: the span and the pool written, the keep list
put back, each participant's `Restore` in the file's order (a participant
that reads past its record, or leaves bytes unread, is a Fatal), the task
arena last, then the overlay line.

## 4a. A build setting: `bof3x.dll` at a fixed address

Not a change of the game's behaviour, so no ledger entry. The owner's call
(2026-10-19), on census 2.2: `bof3x.dll` is linked **without
`DYNAMIC_BASE`**, preferred base `0x10000000` (`CMakeLists.txt`,
`-Wl,--disable-dynamicbase -Wl,--image-base,0x10000000`). A quicksave's task
stacks hold return addresses into the DLL and the span holds pointers into
it, so a quick-load is good only where the DLL sat at the take; with ASLR the
same build loaded `0x510000` apart from two directories (SC). The
**relocation section is kept** (`.reloc`, `0xEDAA0` bytes): when the range is
taken the loader still relocates instead of failing to load - which matters
for the hosted build inside `BOF3.exe` - and the header's DLL-base check
refuses the mismatch either way. `0x10000000 + SizeOfImage 0x7073000` lies
clear of Capcom's range `0x400000..0x987000` and of the draw pool's
candidates below 16 MB.

Measured 2026-10-19 (`objdump -p`; the base from the start-up line
`inject ON Msg_SystemPtr ... -> ours`, the function at DLL + `0x46060`):
before, `DllCharacteristics 0x0140` and a base of `0x5C2D0000` (q_sn's
`launcher`, and the live runs' `this build ... at 0x5C2D0000`) and
`0x54790000` (`launcher_st`) - two directories, two bases; after,
`DllCharacteristics 0x0100` (NX only) and `0x10000000` from both directories
and again from a third start of the first. Not measured: across a reboot.

## 5. The registry

```cpp
void Snapshot_Register(const char* tag, SnapshotSaveFn save, SnapshotRestoreFn restore);
// SnapshotSaveFn:    void (*)(SnapshotWriter&)  - Put(bytes) / Put(value)
// SnapshotRestoreFn: void (*)(SnapshotReader&)  - Get(bytes) / Get<T>()
```

Once per module, at start-up (its `_Inject`); 1..15 printable characters,
unique; a registration after the first F6 / F7, or twice, is a Fatal. `Save`
runs at the take, before the span is copied; `Restore` runs after the span
and the keep list are written, so it sees the snapshot's span. A participant
that needs a live value the span held (a device pointer to release) keeps
its own copy outside the span. Both run on the own stack (section 2).

**Registered now: `ST`** (census 3), our statics that are game state and
neither audio nor graphics: Rand's seed (`crt_rest::RandSeedCell()`),
DIV-0062's free ring (`draw_pool::Free()`, `Count()` entries), DIV-0007's
relocated system message pool (`MsgPool_Moved`, 32 KB under a language
overlay, none without), and DIV-0133/0134's "the release's words stand" flag
with sigma's table rebuilt from the restored Config words
(`sigma::SnapshotRestored`). Its record is about 4 KB (36 KB with an
overlay).

Sigma after a quick-load (RC's question): a save's load (`LoadMenu_Apply`,
`sigma::LoadApplied`) clears "the release's words stand" - the save's words
are loaded - and follows the table. A quick-load does not load a save: it
puts back the Config words the span held at the take, the flag as it was at
the take, and the table built from those words (`Follow`'s words 7 / 8 are in
the span already). For a quicksave taken after a save's load - every one
taken in play - that is exactly the state `LoadApplied` left; for one taken
on the title before any load it is the release's words, as the title had
them. So a fresh process that quick-loads (RC's cross form) ends in the
take's sigma state, not the boot's.

**Owed by stage B** (census 3 and 5): SP-A - `Sound_Banks`, `Sound_Channels`,
the stream and its staging, the music sequencer, the fade's own statics;
SP-G - the texture caches, the CLUT rows' contents, the device state caches,
the layering's last pass. Until they register, a quick-load restores the
game's logic and leaves sound and textures as the moment before it.

## 6. The keys and the history

- **F6**: the take, then **the rotation**: the newest is written whole as
  `BISLPS10.TMP`; then `BISLPS1F.DAT` is deleted and `1E` renamed to `1F`, down
  to `10` renamed to `11` - renames only, top down, a gap passed over (every
  file moves up one); then `BISLPS10.TMP` is renamed to `BISLPS10.DAT`. A
  failed write rotates nothing; a rename that fails stops the rotation where
  it is, with a refusal line (`Quicksave refused: BISLPS1A.DAT could not be
  renamed to BISLPS1B.DAT (error 32)`), every file present under some name
  and no new `10`. Success: "Quicksave OK" through `Overlay_Text`, as F12's
  "Save OK". A held key's repeats ask nothing more.
- **F7**: a load of `BISLPS10.DAT`, the newest: "Quickload", or "Quickload
  refused: " and why (no file, another build, ...).
- **F5** is the quick menu's (stage B, QM). **F12** is unchanged.

## 7. The self-test

`BOF3X_SELFTEST_ONLY=1` in the exe host (the host refuses `BOF3X_SHADOW`,
which compares Capcom's code; this test compares nothing of Capcom's, and the
exe host's span is the players'), `BOF3X_SHADOW=snapshot` hosted. On a
temporary directory:

- a take (through the own stack, a stand-in arena on the heap) written to
  `BISLPS10.DAT`; every page of the span and the pool, and the arena,
  scribbled; the file read back, parsed, restored; the span equal to the
  take's byte for byte except the keep list, which holds the live
  (scribbled) bytes; the pool and the arena equal; the participant's bytes
  back; the game's memory then put back from a raw copy;
- a record with no participant and a participant with no record refused,
  with nothing written; another tag, another DLL hash, another DLL base,
  another `Task_StackTop` refused; the save half's summary still where the
  menu reads it under a refused build; a truncated file refused with its
  save half kept;
- the rotation over sixteen files (each up one, the old `1F` gone, `10` the
  new one), over a gapped set (`10`, `13`, `15`, `1F` become `11`, `14`, `16`
  and a new `10`), and with `1A` held open: the refusal, `1F` deleted, `1E`..`1B`
  moved up, `1A` and below unmoved, no new `10`.

Measured 2026-10-19: passes (exe host, `q_sn\launcher_st`). The hosted run
could not be made: Windows Defender quarantined `bof3x-launcher-dev.exe`
(`Behavior:Win32/DefenseEvasion.A!ml`, events 1116/1117 at 09:28-09:29) in
both the harness copy and `build\`.

## 8. The in-process measurement

Method (RC's `tools/quicksave_check.py` was still being built; done by hand,
`qs_compare.py` in the harness scratch): a route plays, F6 at a frame,
nothing pressed, F7 300 frames later, nothing pressed after; the run under
`BOF3X_STATEHASH` (no skip list), `BOF3X_OPT=none BOF3X_LAYERING=0`, the exe
host, an ini-less launcher in `C:\Users\kerok\q_sn`, a hard-linked copy of
the PC install as the game folder. Each record after the restore is paired
with the record before it of the same `Frame_Counter` (the counter goes
back at a restore and the hash counts that as a new frame).

**Town (worldMapAndAreaTransition's town save, standing; four runs, the last with the fixed base - the DLL at `0x10000000` in play, the same pages).** F6 at
recipe frame 730 (`Frame_Counter` 731): `BISLPS10.DAT` **3,911,297 bytes** -
the save half `0x12B0`, the header `0xA0`, the span `0x5DA000+0x3AD000`
(3,854,336 bytes), the task arena `0x1ABB0C` down `0xBB0C` bytes (47,884: the
committed part of the four slices), ST's record. `.rdata`'s FNV was the same
at the load (`unchanged`). F7 at 1031 back to 731; the game ran on.
300 frames paired; the frame of the restore itself equal on every page;
**the first difference at `Frame_Counter` 732**, on 22 pages, every one of
them inside a skip-list row:

| Pages | What | Frames differing | Owner |
|---|---|---|---|
| `0x7CC000`..`0x7DE000` (19) | `Music_Staging`, then `Music_Events` .. `Music_Data`, `Snd_*` | 299 of 300 | SP-A |
| `0x7CA000` | `D3d_Vertices`, `Font_TexCache`'s surface pointers | 198..299 | SP-G |
| `0x6BC000` | `Sound_Channels`, `Sound_Banks` (and the overlay words) | 46..120 (the first frames) | SP-A |
| `0x7F1000` | `Gfx_PacketPools` | 1..6 | per-frame (the draw) |
| `0x7BF000`..`0x7C2000`, `0x7E1000` | the VRAM shadow, `Effect_Objects` .. `Input_Held` | 1 frame each, one run of four | timing: which frames were drawn (a late frame is not; uploads flush in the draw) |

The raw dumps at `Frame_Counter` 740 before and after the restore
(`BOF3X_STATEHASH_DUMP`, `statehash.py bytes`) differ in **one range: the
73,728 bytes of `Music_Staging`** - every other byte of `.data` equal. So in
the field the snapshot core restores the game's logic exactly; what differs
is the stream's staging, which SP-A refills.

**Combat (the combat route, mid-turn: F6 100 frames into the turn's 405-frame
action).** F6 at `Frame_Counter` 1110, F7 at 1410 back to 1110. The pages that
differed are the texture caches (`0x6C3000`, `0x6C6000`, `0x6C8000`:
`Gfx_TexCache`, the device-state caches), `D3d_CellTexCache` (`0x7CB000`),
the display page and the sound pages - all skip-list rows, SP-G's and
SP-A's. Then, **73 frames after the restore, a crash in `DSOUND.dll+0x2DF88`**
(writing `0x6B431460`, inside DirectSound's image) as the battle loaded
`SND\091_03.DAT` for a spell's sound bank: the restored `Sound_Banks`
records name DirectSound buffers released or replaced after the take.
That is the audio participant's first job (SP-A), not a defect of the span;
until it exists a quick-load across a sound-bank change is unsafe.

**What a restore gets wrong today, and who owes it:**

- SP-A: the music stream (its staging, the stream's events and decoder),
  `Sound_Banks` and `Sound_Channels` - wrong sound at once, and the crash
  above when a bank is reloaded after a restore.
- SP-G: the texture caches (`Gfx_TexCache`, `Font_TexCache`,
  `D3d_CellTexCache`), the device-state caches and the CLUT rows' contents -
  textures as the moment before the load until they are re-uploaded.
- Nothing seen in the game's logic: no page outside the skip list differed
  in the town, and the byte dump agrees.

## 9. Ledger draft A (for the coordinator to number)

- **What the original did:** F6 did nothing; F7 selected the next renderer
  device and showed its name (nothing since DIV-0040). There were no save
  states: F12's `Save_QuickWrite` writes an ordinary save to slot 0
  (`BISLPS00.DAT`), the field's state only.
- **What this does:** F6 saves the game's whole state between two logic
  frames - the span, the task stacks, the registered participants' records -
  to `BISLPS10.DAT`, after moving the existing quicksaves up a slot
  (`1E` -> `1F` .. `10` -> `11`, the old `1F` deleted); F7 restores the newest,
  to the frame. Each file starts with an ordinary save file. A file made by
  another build, or in a process whose DLL or task stacks sit elsewhere, is
  refused with its reason on screen; its save half is untouched. Files
  `BISLPS10.DAT`..`BISLPS1F.DAT` (and a transient `BISLPS10.TMP`) in the game
  directory, about 3.8 MB each.
- **Why:** the owner's design of 2026-10-17 (IDEAS I13, I18;
  [`quicksave-round.md`](quicksave-round.md) section 5): save states restored
  to the exact moment, battles and events included, which F12's
  save-anywhere is not.
- **Reversible?** The keys are ours; without them nothing of the game
  changes. The ordinary save list (`BISLPS0?.DAT`) is draft B's (QM).

## QM. The quick menu (F5) and the two lists

The quicksave round's group QM (branch `quicksave/qm`, 2026-10-19):
`src/game/quick_menu.{h,cpp}` (new), the lists' and the slot draw's bodies in
`src/game/save_menu.{h,cpp}`, one line each in `Game_WndProc` and twice in the
loop (`win_main.cpp`), `InputScript_PausedWord` (`input_script.{h,cpp}`),
`Snapshot_LastLine` (`snapshot.{h,cpp}`), `QuickMenu_Inject` last in
`InjectAll`, and `tools/recipes/quick_menu.txt`.

### QM.1 The two lists

`Save_ListFiles` (`0x4548B0`) and `Save_ReadSummaries` (`0x588DC0`) keep
their entries; their bodies are `save_menu::ListFiles(pattern, table,
bounded, close)` and `save_menu::ReadSummaries(table, summaries, read,
context)`, and `SaveMenu_DrawSlots` (`0x588D20`) is
`save_menu::DrawSlotsOver(x, y, highlight, summaries, first word, cursor
word)`.

| List | Pattern | Table, summaries | Read through | Bound |
|---|---|---|---|---|
| the saves (the original's callers: `TitleFlow_Begin`, `LoadMenu_List`, the inn's `FieldSave_Begin`, DIV-0002's `Save_WriteFile`) | `BISLPS0?.DAT` (the original: `BISLPS??.DAT` at `0x65289C`) | `Save_Directory` `0x929F40`, the summaries `0x905BC0` | `Save_ReadFile` into `Save_Staging`, as the original | sixteen, the rest not stored and logged |
| the quicksaves (the quick menu, at every F5) | `BISLPS1?.DAT` | two tables of ours in `bof3x.dll`'s `.bss` (`0x180` and `0x1C0` bytes), outside the span and in no participant: a listing, read fresh at each F5 | `fopen` / `fseek(0xCA0)` / `fread(0x1C)`: nothing of the game's file layer (`File_Slots`) or `Save_Staging`, both in the span | sixteen, the rest not stored and logged; the search closed |

`name[7]` stays the slot digit in both: `BISLPS13.DAT` is quick slot 3.
A name of `0x14` bytes or more with its NUL (it would run into the entry's
size word) is not stored either, logged. The save list's `_findclose` is
still missing (the original's handle leak, [`save-files.md`](save-files.md)
2, kept); the quick list closes its search.

The switch: `SaveLists`, a `PatchBytes` of a flag of ours set by
`SaveLists_Arm` from `QuickMenu_Inject`, after every module's self-test. So
`save_menu`'s fuzz still compares Capcom's `Save_ListFiles` (the pattern
`0x65289C`, unbounded) and `BOF3X_ORIGINAL=SaveLists` keeps it in play.

### QM.2 The menu

- **F5** (`QuickMenu_Key`, the first line of `WM_KEYDOWN`): opens the menu
  wherever the loop runs (`Frame_Counter` non-zero: not during the intro
  videos), setting `Game_Paused` and calling `Sound_PauseAll` as F9 does
  (already paused by F9: left so). A second F5 closes it. While it is open
  **every key is the menu's** and does nothing - F6, F7, F8, F9 included -
  since any key would otherwise end the pause (and F9 quit to the title).
- **Each paused frame** (`QuickMenu_Frame`, in the loop's paused branch in
  place of the pause lines): the pad through `Pad_Read` (zero while another
  window is in front, DIV-0033), or the playing recipe's word
  (`InputScript_PausedWord`: the paused latch steps a recipe but drops its
  words, so without it a recipe could not drive the menu); then the draw.
- **The look is the title's load menu's** (the owner, 2026-10-19: "the
  background of the save screen ... the same load file yes/no with the
  pointer hand"). The load menu `TitleFlow_Load` (`0x587DB0` step 3,
  `LoadMenu_*` `0x588140..0x5885D0`, ours in `save_menu.cpp`) draws every
  state as: `Menu_DrawBackdrop` (`0x575690`) of the Config's background byte
  `0x903A5B` (the tiled wood of the reference capture; the player's choice
  among four), `Menu_DrawTitleBox` (`0x574AB0`) `(0x14, 0x12, 0x118, 0x13,`
  the window colour `0x903A5A)`, a system message through `Text_DrawAt` at
  `(0x1C, 0x15)` - `0xAC` while choosing ("Load which file?" under
  `en-US`), `0xB9` while asking ("Load game?"), `0xB8` (`0xA6 + 0x12`,
  `LoadMenu_Error`'s error 2) after a failed read - then `SaveMenu_DrawSlots
  (0x20, 0x30, lit)`, and in `LoadMenu_Confirm` `Menu_YesNo` (`0x5747D0`):
  its line, system message `0xF`, at `(0x1C, 0x16)` and `Menu_DrawHand` at
  `0xFE - 36 * answer` (DIV-0027's line function and `0x112 - 56 * answer`
  under a Latin overlay), `y 0x18`. The quick menu draws the same, in the
  same order: `Menu_DrawBlackScreen` (under everything: it covers a wide
  picture's sides, which the backdrop's ten columns do not), then
  `save_menu::DrawLoadWindows(message)` (the load menu's own `LoadWindows`
  and `LoadMessage`), `DrawSlotsOver` over the quick summaries with the
  menu's own first-shown and cursor words, and Menu_YesNo's draw half with
  the menu's own answer. Every word on screen is the game's text, so a
  language overlay translates it; the heading is the load menu's own
  message `0xAC`, not a word of ours.
- **Reusing the draws, not the state.** The backdrop and the box read only
  the Config's two bytes. `Menu_YesNo` itself is not called: it reads
  `Input_Pressed` (not latched while paused), keeps its answer in
  `0x929F0B` and plays sounds - the menu keeps the answer itself and draws
  its line and hand. The load menu's words `0x9036D4` / `0x8034D0` (cursor,
  first shown), `0x6BDF86` (its state), `0x929F0B`, the summaries
  `0x905BC0` and `Save_Staging` are never written. The draws write what any
  paused frame's draw writes (the packet pool, the ordering table, the text
  scratch `0x904BA0`).
- **Slots**: up / down over 0..15 (empty ones included, as the load menu's
  cursor), on the press and then every 4 frames once held 20; the three shown
  follow the cursor. Slot 0 is `BISLPS10.DAT`, the newest: F6's rotation
  keeps the history newest first. Confirm on a slot holding a quicksave
  asks "load game?" with Yes first (`LoadMenu_Choose` sets `0x929F0B` to 1);
  confirm on an empty slot does nothing (the load menu buzzes, `0x107`; no
  sound here); cancel closes the menu.
- **Yes / No**, in `Menu_YesNo`'s order: cancel is No; confirm on Yes loads,
  on No goes back to the slots; left / right (`0xA000`) flip the answer.
- **Confirm and cancel** are the game's words 7 and 8
  (`Field_ConfirmButtons` `0x90358E`, `Field_CancelButtons` `0x903590`), as
  `LoadMenu_Choose` and `Menu_YesNo` test them: they follow sigma (DIV-0133)
  and the release's buttons, cross on PSX-West / PSP-EU since PR #85.
- **A press that leaves the menu acts at its release** (Yes, and cancel on
  the slots): the game's next latch would otherwise see the button newly
  down against the `Input_Held` from before the pause - or, after a load, the
  quicksave's.
- **Yes** asks `Snapshot_RequestLoad(0x10 + cursor)` (SN's entry already
  takes any slot `0x10..0x1F`) and stays open and paused for that frame; the
  core runs the load at the loop's point, and `QuickMenu_AfterPoint` (right
  after `Snapshot_Pending`) reads `Snapshot_LastLine()`. Restored: the span
  holds the take's `Game_Paused`, 0 (a take is never made paused: F6's
  `WM_KEYDOWN` ends a pause first, and the menu takes F6), so the menu
  closes and `Sound_ResumeAll` runs. **Refused** ("Quickload refused: ...",
  the game untouched): the menu stays on the slots, with the load menu's own
  "could not load" message `0xB8` in the title box and the core's reason
  below the slots in the 8 x 8 font, `REFUSED - <the reason>` upper-cased
  (the game has no message for "another build" or "no snapshot"; the font
  draws a colon as one dot). The next question clears it. The core's
  overlay line alone would not show: with our back buffer the loop's
  `Overlay` writes it to the log, not the screen.

`Snapshot_LastLine` is the one addition to SN's files: an accessor of the
line `Show` last wrote (`g_overlay`).

### QM.3 Measured (2026-10-19)

- **Self-test** (`QuickMenu_Inject`: `BOF3X_SELFTEST_ONLY=1` in the exe
  host, `BOF3X_SHADOW=quick_menu` hosted; both from ini-less copies in
  `C:\Users\kerok\q_qm`, exit 0). Two temporary directories of `0xCBC`-byte
  files whose bytes are each one's own mark (no save data):
  A = `BISLPS00..0F`, `BISLPS0G`, `BISLPS10`, `13`, `1F`;
  B = `BISLPS10..1F`, `BISLPS1G`. The save list of A: 16, every entry
  `BISLPS0?`, `0G` logged and not stored, slot *n*'s summary file *n*'s; the
  quick list of A: 3, slots 0, 3 and F shown, the rest empty; of B: 16 of 17,
  `1G` logged. Each table has `0x40` guard bytes after its sixteen entries,
  unwritten every time. **The trap, measured**: B listed with the original's
  `BISLPS??.DAT` shows `BISLPS10.DAT`'s summary as save slot 0; A has 20
  matches under it. The cursor, stepped as a value over slots 0, 3, F
  present: down 1..15 with the first shown `max(0, n - 2)`, no step past F or
  0, up keeping the cursor among the three, a held down = 1 + 3 steps in 30
  frames, a button held at the open not counted, no question on an empty
  slot, the question opening on Yes, right to No and left back, down not
  moving the cursor while asked, No and cancel back to the slots, Yes
  loading only at the confirm's release (slot 3), cancel closing at its
  release. Run again with the load menu's look: exit 0 in both hosts.
- **The fuzz**: `BOF3X_SHADOW=save_menu` hosted, 66,000 rounds (1,500 per
  kind, 44 kinds), 0 mismatches with the bodies split.
- **Live** (exe host, the PC install hard-linked in `q_qm\game`, the `town`
  save in slot 0, `en-US`, ini-less launcher): the capture form of
  `quick_menu.txt`. F5 on the field with no quicksave: `open, 0 quicksaves
  listed`, three empty slots; F5 again closed it. Three F6, 20 frames apart,
  at `Frame_Counter` 617, 637, 657: three files of 3,944,065 bytes; F5: `open,
  3 quicksaves listed`; down, confirm: `LOAD` / `BACK` beside slot 1;
  confirm: `Load of BISLPS11.DAT asked at Frame_Counter 727`, then `F7
  BISLPS11.DAT restored at Frame_Counter 728: back to Frame_Counter 637` -
  the second take, as slot 1 should be; then `closed ... (BISLPS11.DAT
  loaded: Quickload)` and the field ran on. Measured twice (the second run
  with the menu's load as built: open and paused through the point).
- **A refused load** (the same route; a copy of the save in slot 0 put in
  the harness game folder as `BISLPS10.DAT`: a save half with no snapshot):
  F5 `open, 1 quicksave listed`, slot 0 drawn from its summary; Load: `F7
  BISLPS10.DAT refused: no snapshot - a save file with no snapshot after it`,
  `not loaded (...); the menu stays open`, the reason drawn above the slots;
  cancel closed it at `Frame_Counter` 656.
- **With the load menu's look** (the owner's call, 2026-10-19; the same two
  routes, the same build otherwise): the same log lines to the frame - F5
  at 595 `0 quicksaves listed`, F6 at 617 / 637 / 657, F5 at 686 `3
  quicksaves listed`, `Load of BISLPS11.DAT asked at Frame_Counter 727`,
  restored back to 637, `closed ... loaded: Quickload`; and the refusal
  `no snapshot` at 630, the menu open, cancel at 656. On screen: the
  backdrop is the loaded save's Config choice (the `town` save's leaves;
  the title's reference capture has the boot default's wood), "Load which
  file?" over the slots, "Load game?" with the hand on Yes, and on the
  refusal the load menu's "File is damaged" (`0xB8` under `en-US`) with
  `REFUSED - NO SNAPSHOT` below the slots.
- **The reference**: the title's own load menu, `town` in slot 0, on the
  same install (`ref_load.txt` in the harness scratch: start, start, then
  the boot Config's confirm R2 to LOAD GAME and to slot 0): "Load which
  file?", then "Load game?" with the hand on Yes.
- Captures (outside the tree), `C:\Users\kerok\q_qm\caps`: `ref\` (the
  title's load menu: `ref_load_list`, `ref_load_yesno2`); `look_three\`
  (`qm_empty`, `qm_three`, `qm_choice`, `qm_loading`, `qm_loaded`) and
  `look_refused\` (`qm_refused_list`, `qm_refused`, `qm_refused_closed`);
  side by side: `sbs_list_three`, `sbs_yesno` (the title's against the F5
  menu's), `sbs_before_after_three`, `sbs_before_after_refused` (the first
  look against this one). The first look's captures: `run1b\`, `run2\`.

### QM.4 Ledger draft B (for the coordinator to number)

- **Subsystem:** the save list (`Save_ListFiles` `0x4548B0`,
  `Save_ReadSummaries` `0x588DC0`, ours in `src/game/save_menu.cpp`;
  [`save-files.md`](save-files.md) 2).
- **Original behaviour:** `Save_ListFiles` lists `BISLPS??.DAT` into the
  sixteen `0x18`-byte entries of `Save_Directory`, with no bound, and
  `Save_ReadSummaries` gives each entry the slot of its `name[7]`, one hex
  digit.
- **New behaviour:** the save list is `BISLPS0?.DAT`, at most sixteen
  entries (a seventeenth match, or a name too long for an entry, is logged
  and not stored); the two functions take the pattern and the tables, so
  the quick menu lists `BISLPS1?.DAT` into tables of its own. The title's
  load menu and the inn's save menu never list a quicksave.
- **Why:** the quicksaves (`BISLPS10..1F.DAT`, draft A) match the
  original's pattern and their `name[7]` is a hex digit: `BISLPS10.DAT` would
  show as save slot 0 when slot 0 holds no save (and loading it would open
  `BISLPS00.DAT`, which is not there), and sixteen saves plus any quicksave
  overrun the table. The owner, 2026-10-17: the quicksaves are reached from
  F5 only (round doc 5, answer 2).
- **Also in the PSX version?** No: a memory card has no file listing of
  this kind; the PC port's own.
- **Verification:** QM.3.
- **Reversible?** `BOF3X_ORIGINAL=SaveLists` (the original's pattern and its
  unbounded table, quicksaves included in the list).

### QM.5 Ledger draft D (for the coordinator to number)

- **Subsystem:** the window procedure's F5 and the loop's paused branch
  (`Game_WndProc` `0x4FC6F0`, `WinMain` `0x4FCB00`, ours in `win_main.cpp`;
  `src/game/quick_menu.cpp`).
- **Original behaviour:** F5 did nothing but end a pause, as any key does.
- **New behaviour:** F5 pauses and opens the quick menu (QM.2), drawn as
  the title's load menu: its backdrop, title box and messages, the save
  slots - the sixteen quicksaves newest first, three shown - and on a slot
  with a quicksave its "load game?" Yes / No with the pointing hand (an empty
  slot takes no confirm). Yes restores that quicksave to the frame; a refusal
  keeps the menu open with the load menu's "could not load" and the reason;
  No returns to the slots; a second F5 or cancel closes it. The menu reads
  the pad itself while paused and draws with the load menu's draws; it
  writes none of the game's menu state.
- **Why:** the owner's design of 2026-10-17 (round doc 5, answers 2, 3 and
  5): F5 the only way to the quicksaves, load only, sixteen slots newest
  first, three shown as the save menu shows them; and their call of
  2026-10-19 on the first captures: the save screen's background and its
  load yes / no with the hand, in the game's own font and text.
- **Also in the PSX version?** No quicksaves there; ours.
- **Verification:** QM.3.
- **Reversible?** Not a switch: F5 is ours (nothing of Capcom's is
  replaced; without the key nothing changes).

### QM.6 The defect fixed on the way (for the coordinator to number)

**`Save_ListFiles` has no bound** (Capcom's, the PC port's code;
[`save-files.md`](save-files.md) 2): every match of `BISLPS??.DAT` is stored,
`0x18` bytes apart from `Save_Directory` `0x929F40`, so a seventeenth file
writes past the table's `0x180` bytes towards `Save_Staging` `0x92A0E0`, and
each name is copied whole (`strlen + 1`) into a `0x14`-byte field. A player
reaches it with sixteen saves and one more matching file (a copy, or since
draft A any quicksave). Fixed by draft B's bound under `SaveLists`; measured
by the self-test (QM.3: twenty matches in A, sixteen stored, the guard
unwritten). The search handle's leak is not fixed.

### QM.7 Open, for the owner

- Only the refusal's reason is English in the 8 x 8 font; every other word
  is the game's text. The heading is the load menu's own "Load which
  file?" (`0xAC`): the screen does not say "quick". The empty slot's text is
  the game's own (`0x669CCC`: Chinese on an `en-US` run, as the title's load
  menu shows it there too - the reference capture).
- No sound for the cursor or the choices: the game's sound is paused while
  the menu is open, and a cue would write `Sound_Channels` in the span.
- After a menu load, `Sound_ResumeAll` runs after the participants'
  restores: SP-A's participant decides what is playing then.
