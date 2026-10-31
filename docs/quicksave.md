# Quicksaves: the snapshot core (F6, F7, `BISLPS1X.DAT`)

**Status:** BUILT (2026-10-19, the quicksave round's group SN, branch
`quicksave/sn`; built, self-tested headless in the exe host, measured
in-process once - section 8. Stage B added the graphics and audio
participants the same day (sections SP-G and 10) and the F5 menu (QM);
the round merged as PR #95, [`quicksave-round.md`](quicksave-round.md).
Not played by the owner.)

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

> **Amended 2026-10-29 (the relocatable-image round):** the addresses below are VAs. In the exe host the span is read and written at its place, `Img(0x5DA000)`, so a quicksave taken with the image at one base is refused at another as "another span" - measured both ways in the stage C2 proof - and one taken and loaded at the same moved base restores.

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
stack: that frame takes the time (not a worker thread - the hosted self-test
asks for the build id under the loader lock, where a thread cannot start).
A second F6 / F7 before the first has run replaces it, and the log says so.
The temporary file is flushed to the disk before the rename that makes it the
newest.

**A load checks everything before writing a byte**, and a refusal names why
in the overlay and the log, the game untouched: not a save file (under
`0x12B0` bytes); no snapshot (no header); another format; truncated (the
header's size, or a record past the end); another host; **another build**
(the tag or the hash); another process (the DLL's base, or `Task_StackTop`);
another span; **a record whose tag has no participant**; **a participant with
no record** (rule 4: never a partial restore); **another language** (section
4b); **another music source** (section 4c); trailing bytes. The save half
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

## 4c. Another music source

The music file a track reads is the start's too: `BOF3X_MUSIC` (the cache's
sequenced songs or the MP3s, DIV-0087), the cache's layers (DIV-0109) and the
install's own `BGM`. `SOUND`'s record names the take's music file by its
track and size, and `SnapRestore` reads that track again and stops the game
if the file differs - after the span is written (the review of PR #95,
2026-10-21). Since 2026-10-21 `SOUND`'s check (`SnapCheck`) walks the record
to that file and asks this process's size for the track, nothing read
(`MusicFileSize`: the cache song by `music_seq::Probe`, else the MP3 names
`Music_LoadFile` tries): another size, or no file, refuses the load as
"another music source" before a byte is written. The self-test checks the
same file through and a smaller one, and no file, refused.

## 4b. Another language

The language is the start's (`BOF3X_LANG`), not the build's, so the build id
does not see it. Two cases (2026-10-20, the owner's call):

- **The original Chinese text and any translation, either way: refused**
  ("another language"). A translation moves the system message pool into a
  32 KB buffer of ours (DIV-0007) and the original does not, so `ST`'s
  record holds 0 bytes of pool in one and 0x8000 in the other. Before
  2026-10-20 `StRestore` met that as a Fatal **after** the span had been
  written; now `ST`'s check (`StCheck`, a participant's optional
  `SnapshotCheckFn`, run in `Parse` on the record) refuses it before a byte
  is written. The file does not record which translation, only whether one
  ran, so the log line says "a translation", not its tag.
- **One translation into another (say en-US into fr-FR): let through.** Both
  pools are 0x8000 bytes and every check passes. The take's pool and the
  take's area text in the span come back in the take's language, and the
  game's own text stays so until it reloads it from the disc in the running
  language. Read from the code, not measured: how long that lasts and where
  it shows is [`IDEAS.md`](IDEAS.md) I47's to review.

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
  save half kept; a participant whose check refuses its record refused,
  with nothing written; `StCheck` letting `ST`'s own record through and
  refusing it with the pool's presence turned the other way ("another
  language", section 4b);
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

## 9. Ledger draft A (numbered DIV-0145, 2026-10-19)

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

### QM.4 Ledger draft B (numbered DIV-0146, 2026-10-19)

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

### QM.5 Ledger draft D (numbered DIV-0147, 2026-10-19)

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

### QM.6 The defect fixed on the way (numbered D322, 2026-10-19)

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

## SP-G: the graphics participant (`GFX`)

Stage B's group SP-G, branch `quicksave/spg` (2026-10-19):
`src/render/gfx_snapshot.{h,cpp}`, with small accessors in `render_shim`,
`layering`, `map_layers`, `widescreen` and `wide_rand`; the census rows are
[`quicksave-census.md`](quicksave-census.md) 3.1, 4 and 5. Registered at
start-up as `GFX` (InjectAll, beside `Snapshot_Inject`), with a **discard
step** - SP-A's fourth argument of `Snapshot_Register`, adopted as SP-A wrote
it so the two branches merge without a conflict in `snapshot.{h,cpp}`: it
runs once the file is accepted and before the span is written, the one moment
the span still holds the live process's words.

### What is saved, what is kept, what is rebuilt

| State | At F6 (the record) | At F7 |
|---|---|---|
| **The palette rows** `Gfx_ClutRows` (`0x6C2A40`, 512 x {generation, `0x1000`-byte block}) | every generation, and each block's `0x1000` bytes - **carried, not reconverted**: `Gfx_LoadImage` writes the shadow without converting the row (`gfx_image.cpp`), so a row can lag its shadow, and a reconversion would draw what the live game did not | the take's bytes into the live block (the pointer stays the process's - RC's `+4` words); a row with a block then and none now gets one (`Crt_malloc`, as `Gfx_ConvertRow` makes it); a row made since is freed and nulled, so `Gfx_ClutPixels` converts it on first use again, as then; the take's generations |
| `Gfx_TexCache` (`0x6C3F40`), `D3d_CellTexCache` (`0x7CAE38`) | nothing | discard: every texture and surface the live tables hold, in the teardown's order, **released after the next present** (below); then both tables emptied - page and cell textures are built again on demand from the restored shadow and rows. Both tables are skip-listed: the hash sees neither their slots nor their use counters |
| `Font_TexCache` (`0x7C9F50`, 128 x 0x14) | nothing (its words are in the span) | discard: the live surfaces released after the next present; then **every entry the take had built is built again** - `Font_BuildGlyphTexture(slot, glyph, CLUT)` from the take's words and rows - and the take's glyph, CLUT, generation and used words written back, so the cache's lookups and its choice of a free slot go on as they would have (the glyph words are not skip-listed: the hash sees them) |
| the device's pipe state (`render_shim` `g_state`) and the clear colour | `PipeState` without its texture and grab, the colour | as at the take; consistent with `D3d_ShadeModeCache` (`0x6C3A44`), which comes back with the span |
| DIV-0125's frame-buffer grabs | nothing | all forgotten (`render::ForgetGrabs`): the pictures they hold are the live moment's |
| DIV-0071's last pass (`layering` `g_runs`, `g_runs_now`) | the runs the last pass stamped, each with its table index | both tables emptied; the runs written back at their indices **restamped with this process's `g_pass`**, so the next pass's `Defer` judges by what the take's last pass drew |
| DIV-0085's `g_file_sides` (64 KB) | up to its last non-zero byte | as at the take (the rest zero) |
| DIV-0041's `g_split` (22 records), `wide_rand`'s seed and last inset | copied | as at the take |

**The display words kept live** (read by the discard step, written back over
the span by the restore; checked against `symbols.toml` at start-up where it
names them) - what `Display_Setup`, DIV-0042's resize and F8's teardown and
set-up write, beyond SN's keep list: `Gfx_ScreenRect`'s right and bottom and
the device count (`0x66B710`, 12 bytes), `Dd_CellBackdrop` and its pairs
(`0x6BEA00`, 20), `D3d_BackMaterialHandle` (`0x6C3A40`), `0x6C3A48`,
`Gfx_RenderFlags` and the device records (`0x6C3A48`, `0x4F8`), the desktop's
height (`0x6C9F40`), its width with `D3d_ScaleY` / `D3d_ScaleX` (`0x7C9F44`,
12), `D3d_AfterDraw`'s three surfaces (`0x7CADF8`, 12),
`Display_ReleaseOrphans`' six objects (`0x7CAE20`, 24), `DDraw_Primary`,
`DDraw_BackBuffer` and the Z-buffer (`0x7CC338`, 12), the clipper and the
IDirect3D (`0x7CC348`, 8), `D3d_Viewport` and `D3d_BackMaterial` (`0x7CC354`,
8), the back buffer's pitch and `Gfx_PixelFormat`'s three records
(`0x7DED5C`, `0xC4`). Equal to the take's in one process that was not
resized; a quicksave made in a window of another size, or before an F8, must
not bring that moment's objects or scale back. `D3d_ShadeModeCache` is not
kept: it goes with the pipe state.

**Released after the next present** (`render::ReleaseAfterPresent`). The draw
recorded before the load is presented by the next `Gpu_PutDispEnv`, and its
commands name the live surfaces' versions; released at once, each would copy
its pixels into the 8 MB frame arena (`BeforeWrite`; a page texture is
128 KB, and 228..352 objects were released per load in the runs below). They
go right after that present instead. Capcom's DirectX 6 objects
(`BOF3X_ORIGINAL`) are released at once.

**Not touched, by class** (census 3.1): `field_frame.cpp`'s band (per-frame);
SatPixie's accumulation (DIV-0043: the pre-load picture's ghost fades over the
next frames, under that look only; not measured); the Direct3D 11 backend's
per-surface textures follow the surfaces (released ones are swept at the next
present, new ones get new GPU textures).

**SC's `g_pass += 2`, verified by reading and not taken:** with the history
moved two passes on, the next `Defer` finds no run stamped `g_pass - 1`, and
`RunsClear` answers false for a layer whose cells hold a record run ("was not
drawn last pass") - the sprite is kept in its layer there, not judged by the
proximity test the census expected (that is `g_runs_before == 0`'s path).
Carrying the runs is exact and costs a few KB.

The record: 15,067 bytes in the self-test (no palette rows); 17 rows, about
80 KB, in the town and the house; 25, about 116 KB, on the world map.

### The checks

**Self-test** (`BOF3X_SELFTEST_ONLY` in the exe host, `BOF3X_SHADOW=gfx_snapshot`
hosted): a take with rows 3, 40, 480 holding blocks and 41 none; then the
live moment - row 40 freed, 41 made, 3 and 480 rewritten, generations moved,
three stand-in surfaces in the three caches, the kept words scribbled; the
discard and the restore as the core runs them. Checked: every generation and
block as at the take (40's made again, 41's freed, 3's live block kept); the
six cache objects deferred and **not** released before the present, released
once each after it; the page and cell tables empty; the kept words the live
ones; the record read to its end, and a take after the restore writing the
same record. Passes (2026-10-19, `q_spg\launcher` and the combined build
below). Not covered headless: the glyph rebuild (no glyph table before the
game runs) - the live check covers it.

**`BOF3X_FRAMEHASH=1`** (instrumentation, `render_shim.cpp`): at each present,
one log line per **walk of the lists** - the commands appended under one
`Frame_Counter` - with a 64-bit FNV-1a over them in order: the pipe state but
its pointers, the vertices, and for a textured draw the **content** of the
texture version sampled (size, format, colour key, the pixels within the
width) - never an address, so a walk drawn with rebuilt textures hashes as the
live one when the pixels agree. Per walk, not per present: a present carries
two walks when two logic frames passed between draws (`Gpu_PutDispEnv`
presents only when the display buffer changed). The clears are left out: they
follow a present, whose timing is not the lists'. `tools/quicksave_frames.py
LOG` pairs the walks tagged C + k after the F7 with those after the F6 (the
walks drawn before the load and presented after it are passed over) and
reports the first frame and the first difference.

**The measurement.** `tools/quicksave_check.py run` on RC's proof recipes
from ini-less launcher copies in `C:\Users\kerok\q_spg`, the exe host, the PC
install hard-linked into `q_spg\game`, `--lang en-US`, `BOF3X_FRAMEHASH=1`,
`BOF3X_SHOT_EVERY` (100, or 75) into `BOF3X_SHOT_DIR`, under the machine lock.
Builds: **before** = `quicksave/round` `a8002f6f` (no `GFX`); **after** = this
branch; **with SP-A** = this branch merged with `quicksave/spa` `5c6c82b3` in
a scratch tree (nothing committed) - after alone, the missing sound
participant ends every town run 36 ticks after the load (`SndBuf_Play` on a
stale `Sound_Banks` buffer) and the world map's after 175 (`Music_SetVolume`
.. `FinishStoppingFade`, in ntdll's heap). Conditions: **id** =
`BOF3X_OPT=none BOF3X_LAYERING=0`; **def** = the player's defaults (layering
on); **wide** = defaults with `BOF3X_WIDE=1`. "k" is ticks after the F7.

| Route (F6 / F7) | Build, condition | State hash after F7 | Frames after F7: the first; equal / compared | Captures, after F6 against after F7 |
|---|---|---|---|---|
| worldMapAndAreaTransition (595 / 895) | before, id | equal for the 36 ticks before the sound crash | - (no frame hash) | - |
| | before, def | from k=12: `0x6BC000`, `0x7DE000` (sound), the VRAM shadow 1 tick; crash at k=43 | - | t000600 / t000900: 0 pixels differ (the load stays in area 7, and the stale caches happen to hold the right textures) |
| | after, id / def / wide | id from k=4 (`0x6BC000`, `0x7DE000`), def and wide equal - to the sound crash at k=36 | equal; 33 / 33, 35 / 35, 35 / 35 | 0 pixels |
| | with SP-A, id / def | from k=0, `0x7DE000` only (SP-A's page) | equal; **299 / 299** both | 0 pixels |
| worldMap (1270 / 1570) | before, id / def | **crash at k=0..1 in `render_shim`'s `WriteRaw`** (writing through a null pixel pointer: a build into a texture the restored `Gfx_TexCache` named, released since) | - | - |
| | after, id / def | equal, 175 ticks to the sound crash | equal; 174 / 174 both | t001275 / t001575: 0 pixels (t001500, before the load: 307,200) |
| | with SP-A, id / def | from k=0, `0x7DE000` only | equal; **299 / 299** both | 0 pixels |
| cutsceneAndNue (2165 / 2465) | before, id / def | from k=1: `0x6BC000` (k 1..120, the overlay line and sound), `0x7DE000` | - | **674 pixels: the typing line reads "Ihat the lonstei / thatb" for "That the monster / that'"** - the restored `Font_TexCache` words over the live glyph surfaces |
| | after, id / def | from k=1, the same two pages | equal (per present, before the walk split: 4 / 290 and 3 / 291 differed, each a present carrying two walks) | 0 pixels |
| | with SP-A, id / def / wide | from k=1: `0x6BC000` (k 1..120), `0x7DE000` (k=1 only) | equal; **298 / 298** in all three (a first run per walk with the clears hashed differed by one command in 14 / 281 and 1 / 298 walks: the clear after a present, hence the clears left out) | 0 pixels |
| worldMap, wide | with SP-A, wide (`BOF3X_WIDE=1`, 852 x 480) | **equal, all 300 ticks** | equal; **299 / 299** | t001275 / t001575: 0 pixels (t001500: 408,959) |

Every "after" page is SN's (`Overlay_Text` / `Overlay_Frames`: the
"Quickload" line is written after the span, 120 frames) or SP-A's
(`0x6BC000`'s sound words, `0x7DE000`), or noise RC's references show between
two plain runs (quicksave-routes 5: a VRAM-shadow page and `0x937000` for a
tick, an upload landing a frame apart; `Font_TexCache`'s page `0x7C9000` for
two ticks, in a run with 18 late frames). No page of SP-G's differs.

Captures (game-written frames: game data, outside the tree, never for the
repository): `C:\Users\kerok\q_spg\runs\<build>_<cond>_<route>\shots\t<FRAME>.bmp`,
`.png` copies beside the compared ones; the before pair of the typing line
cropped together, `runs\before_id_cutsceneAndNue\shots\pair_text.png`. Logs and
hashes beside them: `bof3x.log`, `quick_<route>.sh`, `check.txt`, `frames.txt`.

### What still differs, and whose

- **The first presented frame after F7 is the pre-load picture**: the draw
  made before the load, presented by the next `Gpu_PutDispEnv`. One frame,
  inherent to the point (the core restores after the logic, before the next
  draw); the restored lists are presented the frame after.
- **Frame-buffer grabs** (DIV-0125) are forgotten: a quicksave taken while a
  grab is live (Depress's swirl, the fishing strip) draws Capcom's port's
  picture - the shadow under the grab - until the effect grabs again. Not on
  a route; carrying a grab means reading its GPU texture back at the take.
  SP-G's, open.
- **Cache quirks rebuilt fresh:** a cell texture whose CLUT changed while it
  was used that frame keeps stale pixels in the live game (`D3d_CellTexture`
  does not refresh it), and a page texture over cells that
  `Gfx_LoadImageIfChanged` rewrote is never invalidated; after a load both are
  built from the current data. Not seen on the three routes.
- **SatPixie's ghost** of the pre-load picture under DIV-0043's look: a few
  frames, that look only; not measured.
- Not run: the cross-process forms, the hosted build, the combat, shop and
  music routes, a load across an F8 or a window resize (the kept words are
  designed for it; unmeasured).

No ledger draft: the picture after a load is part of draft A's quick-load;
`BOF3X_FRAMEHASH` is instrumentation.

## 10. The audio participant `SOUND` (SP-A)

Stage B's group SP-A, branch `quicksave/spa` (2026-10-19): `src/game/sound.cpp`
(the participant and its self-test), one optional hook in SN's registry
(10.2), and DIV-0129's flag read and set beside its row in
`save_menu.cpp`. Built, self-tested headless in the exe host, measured
in-process on three routes (10.5). **Not heard by the owner.**

### 10.1 What a restore must do for sound

The span holds the game's words about sound, but most of what they name
belongs to the process (census 4 and 5): DirectSound buffers (`Sound_Banks`'
voice entries, `Sound_Channels`, `SndStream_Buffer`, `Music_Buffer`,
`Music_Notify`, the two events), heap blocks (each bank's sample data at
record `+0x380`, the last stream's file `0x6BDE44`, `Music_File`,
`Music_Data`), the MP3 decoder and DIV-0087's synth. Written back as the take
had them, they name buffers and blocks that a later load has released or
freed: **section 8's combat crash** (`DSOUND.dll+0x2DF88`), and in this
round's "before" runs (10.5) heap corruption in `ntdll` as the next bank,
stream or BGM file was loaded - each such load frees the take's stale
pointer a second time.

### 10.2 Two halves around the span write

A participant's `Restore` sees the span already written, so the live
pointers are gone by then. `Snapshot_Register` takes an optional fourth
argument, a **discard**, which `Restore` (`snapshot.cpp`) calls for every
participant in the file **after the file is checked and before the span is
written**. Nothing else of SN's changed (`SnapshotDiscardFn`, a field of the
registry's entry, one loop): SP-G may want the same for its surfaces.

- **Discard** (the live words): for every bank whose data word is set, each
  voice entry's buffer that `Snd_LoadBank` made (`SndBuf_Release`: stopped,
  released) and the sample data freed - what a reload of that bank does;
  `SndStream_Stop`; `Music_Release` (the ring, its notification and events,
  the file's copy, the decoder, the synth stopped); the last stream's file
  and `Music_File` freed. `Snd_Device` and `Snd_Primary` are the process's
  and are kept, by value.
- **Restore** (the take's words): the device put back; every stale pointer
  cleared; then, in order -
  1. **the banks**: each loaded bank's sample data from the record into a
     new block, the 64 voice entries rebased onto it (same offsets), and a
     buffer made for each from its own wave by `SndBuf_FromWave` - so
     `0x5A69E0`'s buffer, as `Snd_LoadBank` makes them. The cues and the
     rest of each record come from the span untouched;
  2. **the channels**: each of the 23 names the new buffer of the entry its
     old buffer belonged to at the take (found by the take's buffer word
     among the entries); a channel holds only a looping voice
     (`Sound_PlayEffect`), so one that was playing plays again, looping, at
     its frequency and play position (`GetFrequency` / `GetCurrentPosition`
     at the take). **A one-shot voice playing at the take is cut**, as the
     round doc allows;
  3. **the last stream's file** from the record, and a voice stream
     (`SndStream_Buffer`, kind > 0) playing from it at the take made again by
     `SndStream_Play` at its position (stopped if it had stopped: the inn's
     and the camp's waits read it through `Sound_StreamDone`);
  4. **`Music_File`** read again: `Music_LoadFile(Music_LoadedTrack)` (the
     cache's song under DIV-0087), refused with a Fatal unless size, loop
     flag and track come back as the span had them;
  5. **the music stream**: `Music_Start` of the same stream - `Music_File`'s
     track, or the record's copy of `Music_Data` for a `Sound_LoadStream`
     stream - moved before its first half is decoded to **the sample heard at
     the take** by `ResumeSkip`, DIV-0092's own mechanism (the MP3 decoded
     from the top and dropped, bit-exact; the synth by `FastForward`); the
     heard sample is the play cursor's (`HeardFolded`, the code `Remember`
     used, now shared), folded into the first pass and loop as DIV-0092
     folds it. Then `Music_SetVolume` from the span's `Music_Volume`; a
     once-only stream that had played past its end gets zeros in both halves
     and `Music_Finished` set, so the pump stops it at its next notification
     as it would have; a stream that was stopped is stopped again;
  6. **our statics**: DIV-0092's position (`g_resume`: remembered and
     pending), DIV-0028's `g_fade_frame` and `g_stop_unseen`, DIV-0132's
     `g_unducked`, DIV-0129's `g_stream_missing`;
  7. `Snd_BufferDesc` .. `Snd_WaveFormat` (`0x7DE378`, 0x3C bytes, every
     `CreateSoundBuffer`'s scratch) put back as the span had them: the
     rebuild wrote them once a buffer, and the first run showed them on page
     `0x7DE000` until the game's next buffer (115 ticks in combat).

**Rebuilt, not copied:** the decoder, the synth, the ring and
`Music_Staging` (`Music_CreateBuffer` and the pump refill it), the measured
loop's position (`g_loop`, by the skip), DIV-0092's ring bookkeeping
(`g_ring`), every buffer. **Saved:** each bank's sample data (a copy of the
heap block; its size from the toolchain's allocator, `_msize` - every
`Crt_malloc` is `std::malloc` since crt-rest 2.6), the last stream's file,
a non-track stream's `Music_Data`, the channels' entries and play state,
the voice stream's, the music's kind, play state, loop flag and heard
sample, our six statics. SC's eight rows: `g_loop`, `g_ring` rebuilt;
`g_resume`, `g_fade_frame`, `g_stop_unseen`, `g_unducked`,
`g_stream_missing` saved; `music_seq`'s `g_s` rebuilt by `LoadFile` and
`FastForward` (not exercised live: no cache on the measured install).

**Which file each bank came from is not recorded**, as SC suggested: the
block is copied instead, which needs no hook in `Snd_LoadBank`
(`display_env.cpp`) or `Snd_LoadBankFile` and holds under DIV-0089's cache and
DIV-0113's layers alike. It costs the record **623,096 bytes** in the combat
route's battle (six banks) and 1,161,348 in `musicStopCheck`'s; the file was
4,652,284 bytes in combat against 3,944,065 before. Recording (file, layer,
tag) instead is IDEAS I45's (the size), not needed for correctness.

The record's version is 1; a record of another version is a Fatal (the
build check refuses another build first).

### 10.3 Refusals and Fatals (rule 4)

A restore with no discard before it, a bank whose data word in the span is
not the record's, a bank with data and no bytes, a voice entry pointing
outside its bank's data, a channel naming a bank or entry out of range, a
`Music_File` that does not read back as the take's, a track stream with no
file: each a Fatal naming it. At the take: a synth stream that is not
`Music_File`'s track is a Fatal (it cannot happen: `IsSong` holds only for
it); a channel naming a buffer no entry holds is logged and comes back
silent.

### 10.4 The self-test

In the exe host's `BOF3X_SELFTEST_ONLY` start (beside SN's), hosted under
`BOF3X_SHADOW=sound`. The participant's own `Save`, `Discard` and `Restore`,
with SN's span write played by hand between them (the take's words written
back), on stand-ins: DirectSound buffers with a vtable of ours (status,
frequency, cursor, play flags, releases counted), the heap through
`std::malloc`, `Music_LoadFile` and `Music_Start` recorded.

- **Fades** - a DIV-0132 duck five frames in (then the unduck), a fade to a
  stop four steps in (DIV-0143's state, `musicStopCheck`'s), one taken before
  its first tick (DIV-0028's unseen stop), a fade in half way: each saved,
  run on, everything of ours scribbled, restored and run again; the two runs
  equal tick for tick in volume, count, track, stops and every
  `Music_SetVolume`, the stopping ones stopping once.
- **Banks** - two banks loaded by `Snd_LoadBank` from synthetic payloads, two
  looping voices playing on channels 5 and 9 and a stopped one on 7, a voice
  stream; after the take bank 1 reloaded (its buffers released, its data
  freed: the crash's case) and bank 2 loaded; the restore: every buffer of
  the take and of the live process released exactly once, the heads and
  sample data of banks 1 and 3 equal to the take's, bank 2 unloaded, every
  voice rebased and its buffer made anew from its own wave (hash of the data
  chunk equal), the channels on the new buffers, the two looping ones
  playing with loop set at their frequency and position, the stopped one
  not; the stream's file and its voice playing again at its position.
- **Music** - a track stream with the play cursor `0x800` bytes into the
  ring's second half: `Music_LoadFile(200)` called with `Music_File`
  cleared, `Music_Start(Music_File, 1000, 0)` with the skip armed at the
  folded heard sample (55,296), the volume set from the span, DIV-0092's
  position back; and the same with the stream stopped, which is stopped
  again once.

Measured 2026-10-19: passes (`q_spa\launcher`, the release launcher, no
ini). A mutation (the fade frame restored one off) failed seven of its
checks, the frame comparisons among them.

### 10.5 The measurement

`tools/quicksave_check.py run tools/recipes/quick_<r>.txt` then its
`inproc` analysis; the exe host, the PC install hard-linked into
`C:\Users\kerok\q_spa\game`, an ini-less release launcher, `BOF3X_OPT=none
BOF3X_LAYERING=0`, the 173-range skip list, `BOF3X_MUSIC_LOG=1`. "Before" is
the round's tip `a8002f6f` (no participant); "after" this branch.

| Route (F6 / F7) | Before | After |
|---|---|---|
| `quick_combat` (900 / 1200) | first difference k = 1 (`0x6BC000`, `0x7DE000`); **crash** in `ntdll` heap code (`0xC0000005` at `ntdll+0x4FFCC`) after the window, at recipe frame about 2028 (some 830 frames after the restore), the first `DAT\COMN_SE.DAT` load since (a bank: `Snd_LoadBank` freeing the take's stale block) | **no crash**: the 300-tick window and the route to its end (battle won, track 165 at f2501); `SND\091_03.DAT` loaded twice after the restore. First difference **k = 1**: `0x6BC000` for 120 ticks (`Overlay_Frames`, SN's "Quickload" line) and `0x7DE000` at k = 1 only (`Gpu_DispEnv` / `Gpu_DrawEnv` `+2`: the display's buffer parity) |
| `quick_musicStopCheck` (2808 / 3108) | first difference k = 1; **crash** in `ntdll` heap code at k = 30, f2838, as `Music_LoadFile` read `BGM\091.DAT` (the stale `Music_File` freed); DIV-0092's position gone - track 91 from the top, fade 10 | first difference **k = 1**, the same two pages; the fade to a stop completes at f2814 as live; **track 91 resumes at sample 479,232 with the 30-frame fade, as live**; then at k = 81 a Fatal `render: Release of a surface with no references` (SP-G's surfaces, not sound) |
| `quick_shop` (1110 / 1410) | first difference k = 1; **crash** at k = 67 (a call through a null pointer, area 7 mode 7) | **no crash**, the 300-tick window; first difference **k = 1**, the same two pages |

The raw dumps (`BOF3X_STATEHASH_DUMP`) at k = 1 and k = 10 or 20 on each
route, byte for byte: what differs on the sound pages is **only pointer
words** - `Sound_Banks`' sample and buffer pointers and data words,
`Music_File`, `Music_Events`, `Music_Buffer`, `Music_Notify`, `Music_Data`
(every one inside a skip row) - and `Music_Staging`'s 73,728 bytes
(refilled). No sound word the game reads as a number differed:
`Music_FadeCount`, `Music_FadeStep`, `Music_FadeStops`, `Music_Volume`,
`Music_Track`, `Music_Loops`, the kind word and the banks' cues all equal.
The rest of each dump's differences are SN's overlay and SP-G's rows
(`Gfx_ClutRows`, `Gfx_TexCache`, `D3d_CellTexCache`, `D3d_Vertices`,
`Gpu_DispEnv` / `Gpu_DrawEnv`).

**The music's position.** `quick_combat` again with `BOF3X_WAV_DIR` (the
dump goes on through the counter's jump back; its clock is the run's frames,
`docs/wav-dump.md` 1). The take held battle track 151 at heard sample
695,899, folded to 695,808 (the frame's start); the restore started it there
(`asked 695808`). The dump's half-second windows from k = 0 to 285 in steps
of 15, after the take against after the restore, cross-correlated over +-1 s
(`scratch\wav_lag.py` and an inline loop): every window where the music is
alone peaks at **lag +3,140 samples (71.2 ms) with correlation 1.000** (k =
30..105, 150, 255) - the same music sample for sample, 71 ms later after the
restore than after the take; windows carrying the battle's effects peak at
lag 0 (the replayed effects land on the same frames). **Within a buffer's
length** (the ring's 36,864 samples, 836 ms) and within a fifth of a half.
The 3,140 is the dump's measure of where the device's play cursor stood
against the dump's own model of the queue at the take (the resume takes the
cursor; the dump plays the queue at the frames' pace), plus the 91 samples
of the frame floor; not separated further.

Run folders, logs, dumps and crash dumps: `C:\Users\kerok\q_spa\runs\`
(`base_*`, `after_*`, `after_*_3` with the dumps); the scripts in
`C:\Users\kerok\q_spa\scratch\`. Game-derived, outside the tree.

### 10.6 What still differs, and whose

- **SP-G:** the texture caches and CLUT rows (pages `0x6C3000`..,
  `0x7CB000`..), the display's buffer parity for one tick (`Gpu_DispEnv` /
  `Gpu_DrawEnv`), and the `render: Release of a surface with no references`
  Fatal 81 frames into `musicStopCheck`'s window.
- **SN:** `Overlay_Text` / `Overlay_Frames`, written after the span by
  design (the "Quickload" line), 120 ticks.
- **Sound, by design:** a one-shot effect playing at the take is cut; a
  voice stream playing an older file than the last stream's (a kind-0
  stream loaded over it while it played) is cut, logged; the MP3 on a
  rewinding track resumes at the start of the frame holding the heard
  sample (26 ms at most early, DIV-0092's fold).
- **Not measured:** the cross-process form; an install with DIV-0087's
  cache (the synth's `FastForward` path is DIV-0092's, tested there);
  a quick-load during a voice stream (no route holds one).

### 10.7 Ledger: an addendum to draft A (taken into DIV-0145)

No behaviour of the game changes outside a quick-load. Inside one, draft A
should say what sound does, so that "what a restore gives back" is on the
record: *"Sound after a quick-load: the music resumes at the sample that was
heard at the take (for an MP3 on a track that rewinds, the start of its
26 ms frame), at the take's volume and fade; looping sound effects and a
playing voice stream resume at their positions; a one-shot sound effect
playing at the take is not resumed."*

