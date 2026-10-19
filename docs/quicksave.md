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

### 10.7 Ledger: an addendum to draft A (for the coordinator)

No behaviour of the game changes outside a quick-load. Inside one, draft A
should say what sound does, so that "what a restore gives back" is on the
record: *"Sound after a quick-load: the music resumes at the sample that was
heard at the take (for an MP3 on a track that rewinds, the start of its
26 ms frame), at the take's volume and fade; looping sound effects and a
playing voice stream resume at their positions; a one-shot sound effect
playing at the take is not resumed."*

