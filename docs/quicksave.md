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

**The restore**, once checked: the span and the pool written, the keep list
put back, each participant's `Restore` in the file's order (a participant
that reads past its record, or leaves bytes unread, is a Fatal), the task
arena last, then the overlay line.

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

SECTION_8_PLACEHOLDER

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
