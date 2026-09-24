# Window modes — WinMain, WndProc and the FMV player ours

**Status:** IN PROGRESS (2026-09-23). Step 3 of [`display-overhaul.md`](display-overhaul.md)
§5: window modes without a mode-set, the game running unfocused (I12), the
FMVs into the window. Built and checked by the batch of §4; the owner's eye
on the borderless window, F8 and the FMVs is owed (§5).

## 1. What was taken over, and why these

The display overhaul's step 2 put the picture in a render target of our own
([`render-backend.md`](render-backend.md), DIV-0031) and presented it at the
largest integer scale the window's client area holds. The owner's first look
at "fullscreen" with it (2026-09-23) showed the limit of stopping there: the
original WinMain still made its *captioned* window at the desktop's size, so
on a 3440 x 1440 desktop the client was 1440 rows less a title bar, three
times 480 no longer fit, and the picture sat at two times with the bar
showing. The window is WinMain's, so the window modes are WinMain's takeover.

Six functions, all porting-house code with no PSX counterpart, read whole on
2026-09-23 (`python tools/pe_disasm.py 0x4fcb00:450 0x4fc6f0:400 0x59e360:200`;
every branch is in [`symbols.toml`](../symbols.toml)):

| Function | Address | Bytes | File |
|---|---|---|---|
| `Game_WinMain` | `0x4FCB00` | 0x52A | `src/game/win_main.cpp` |
| `Game_WndProc` | `0x4FC6F0` | 0x3C4 | `src/game/win_main.cpp` |
| `Cursor_Sync` | `0x4FCAC0` | 0x3D | `src/game/win_main.cpp` |
| `Display_WindowMoved` | `0x5A5130` | 0x2E | `src/game/win_main.cpp` |
| `Display_DeviceName` | `0x5A6690` | 0x14 | `src/game/win_main.cpp` |
| `Fmv_Play` | `0x59E360` | 0x183 | `src/game/fmv_play.cpp` |

Named on the way and left as Capcom's, each with a typed entry: `Disc_Probe`
`0x5A72C0` (the CD-ROM search WinMain refuses to start without),
`Game_Init` `0x4FD110`, `Gfx_LinkOTags` `0x4FD290`, `Task_SetStackBase`
`0x5A9907`, `Display_TextOut` `0x5A66B0`, `Display_ErrorBox` `0x5A6720`,
`Display_Teardown` `0x5A6380`, `Sound_Shutdown` `0x5A6980`,
`DInput_Shutdown` `0x5A9690`, `DInput_Init` `0x5A94C0`, `Sound_PauseAll`
`0x587C30`, `Sound_ResumeAll` `0x587B90`, `Save_QuickWrite` `0x5809C0`; and
the loop's globals `0x6BC620`..`0x6BC640`, the desktop's size, the pause
lines and pad 2's three words. `Fmv_EnterFullscreen` `0x59E4F0` is retired:
nothing of ours calls it.

## 2. What WinMain is

The original, in order (the evidence field of `Game_WinMain` has every
call): `Disc_Probe`, or the insert-the-disc box and exit; the window class
(`Bof3`, black brush, `BOF3_ICON`); `Cfg_Load`; the desktop's size; the
window - fullscreen at the desktop's size, windowed a 640 x 480 client
centred, both with style `0xCA0000`; `Cursor_Sync`; the two FMVs;
`Display_Setup`; `Game_Init`, whose tail jump into `Task_SetStackBase`
reads `esp` - **the four 16 KB task stacks hang from WinMain's own frame**,
which is why the loop's locals are a few dozen bytes; then the loop:

1. Quit flag; `Input_Latch` unless paused; one `PeekMessage`; **nothing
   more while `App_Active` is 0**; the restart flag (F9's quit to the
   title: `Task_SetStackBase` and task 0 again).
2. If the clock is before the deadline: `Gpu_PutDispEnv`, `Gpu_PutDrawEnv`,
   the two flushes, `Gfx_DrawOTag`, an overlay if one is up. Otherwise the
   frame is skipped.
3. `Sound_Tick` and the clock until the deadline; the fps string once a
   second; the deadline plus 33.334 in double, stored as a float, less 2^32
   past it; the buffer flip, `Gpu_ClearOTagR`, `Gfx_BeginFrame`,
   `SpriteCell_Reset`; `Task_RunAll`, or the pause lines; `Gfx_LinkOTags`;
   `Frame_Counter`.

Ours keeps that order call for call, so a logic frame asks Capcom's code for
exactly what the original's did - the frame hash is the check (§4) - and
differs in four ledgered ways.

## 3. The four divergences

- **DIV-0032, the window modes.** Windowed: a resizable
  `WS_OVERLAPPEDWINDOW` with a 640 x 480 client to start, centred as the
  original centred it; the backend presents at the largest integer scale the
  client holds, so dragging the window bigger is the scale control.
  Fullscreen (`Cfg_Fullscreen`, `BOF3.CFG` line 1, F8): a borderless
  `WS_POPUP` window covering the monitor the window is on. F8 changes the
  style and placement and nothing else; coming back from borderless it
  restores the windowed placement the player had. F7 cycles the device name
  and re-makes nothing. The three overlays (F7's name, F11's frame rate,
  F12's "Save OK") were `TextOut` through the back buffer's device context,
  which the backend's surface has not: they go to the log as `overlay`
  lines when the back buffer is ours, and are drawn as before under
  `BOF3X_ORIGINAL=Display_Setup`.
- **DIV-0033, running unfocused** ([`IDEAS.md`](IDEAS.md) I12).
  `WM_ACTIVATEAPP`'s deactivation no longer clears `App_Active` or pauses
  the sound. The DirectInput keyboard is `DISCL_BACKGROUND` (`DInput_Init`),
  so an unfocused loop would read keys typed into other windows: the six
  pad words are zeroed after `Input_Latch` while the window is not the
  foreground application (`src/hook/input_script.cpp`, `DeviceLatch`, which
  the recipe's and the recorder's latches go through too, before they put
  their own words in). `BOF3X_BACKGROUND=0`, the launcher's "Keep running
  when the window is not in front" box, `background=0` in `bof3x.ini`:
  the original's freeze.
- **DIV-0034, the debt clamp.** A deadline more than 500 ms behind the
  clock restarts at now + 33.34, as at the loop's start, with one
  `DIV-0034` log line: a title-bar drag, a resize or a suspend no longer
  replays its length as unrendered logic frames. Under 500 ms the original's
  catch-up stands.
- **DIV-0035, the FMVs into the window.** `Fmv_Play` never calls
  `Fmv_EnterFullscreen`: no second DirectDraw, no `SetDisplayMode`. The
  destination rectangle is the client's largest integer multiple of
  640 x 480, centred, black around it (a smaller client gets the largest 4:3
  fit); in a 640 x 480 client that is the original's `0 0 640 480`. The
  open with its disc-root retry, `Fmv_WndProc`'s subclass, the play, the
  modal pump, the skip, the stop and close are unchanged. The owner chose
  this over I7's bundled decoder on 2026-09-23; I7 stays on the list.

The ledger entries have the original behaviour and the evidence for each
([`DIVERGENCE.md`](DIVERGENCE.md) DIV-0032..0035).

## 4. How it was checked

Neither WinMain nor WndProc can be fuzzed against a clone - they are the
loop and the message procedure - so the check is live, and the plan's rule
applies: a live A/B of the oracle, not a fuzz ([`display-overhaul.md`](display-overhaul.md)
§4a). `analysis/validate_wm1.sh`, log `analysis/attract/wm1_batch.log`,
2026-09-23; the game runs from a scratch copy of the launcher with a
windowed `bof3x.ini`, and the harness's new `--no-front` leaves it
unfocused for the whole batch (DIV-0033 is what makes that possible; the
oracle, the dump and the hash read memory, not pixels):

0. Self-tests headless: every module at 0 mismatches, 796 ours.
1. A borderless start-up from a second copy with `display=fullscreen`:
   the window's placement, the two FMVs' destination rectangles and the
   backend's set-up in the log; 25 seconds, then ended.
2. The oracle: `attract_run.py --no-front` for 7 minutes against
   `orig_a.tsv`, `mem_dump.py` beside it against `clutref_a`.
3. The frame hash: 6 minutes under the tracer. Against `ab27_orig` **every
   frame differs with every count equal** - and that is the takeover
   changing the hash's content, not a regression: the hash is of (entry,
   caller) pairs, and a call the loop makes into Capcom's code was
   recorded with its return address inside WinMain when WinMain was
   Capcom's and unowned; now WinMain is an owned range, so that caller is
   the owned-caller marker on both sides (`calltrace.cpp`, the owned-range
   rule), which is a different hash for the same calls. So the reference
   is re-recorded, as every logic-function takeover has had to
   ([`HANDOFF.md`](HANDOFF.md) "Pick up here" 1): `wm1_orig` and
   `wm1_origb` at `--original "*,-Game_Clock"`, 6 minutes each, from the
   same scratch launcher (`analysis/attract/wm1_rerecord.log`) - **with
   the foreground held**: the first attempt used `--no-front` and made 0
   logic frames in 16 seconds, because with everything Capcom's the WndProc
   that freezes the loop is Capcom's too (HANDOFF Traps). Only all-ours
   runs can go unfocused.

Results, 2026-09-23 (`analysis/attract/wm1_batch.log`):

| Check | Result |
|---|---|
| Self-tests | 0 mismatches, 796 ours |
| Borderless start-up | window 0,0 3440 x 1440; both FMVs at 760,0 1920 x 1440 (three times); backend window 3440 x 1440, target 640 x 480 |
| Oracle, 7 min | identical at every one of 7,478 compared frames: `Rand` count, message, area |
| Memory dump | arena and VRAM identical; `clut` one row (482) differs - the documented pre-DIV-0022 reference artefact (Traps), not a regression |
| Frame hash vs `ab27_orig` | 10,319 of 10,319 frames differ, every count equal (above) |
| Frame hash, re-recorded pair | Blocked that afternoon - every traced all-original run ended 16 s in; **found and fixed the same evening (§4a)**: the tracer's own single step over a `pushfd`. Re-recorded as `wm1b` (§4a). |

### 4a. The 16-second exit: the tracer's trap flag, saved by `pushfd`

Found 2026-09-23 evening, in the review session. The exit trace's stack
(`0x5BE840 0x5BE799 0x5BA15F 0x5BA057 ...`) reads, disassembled: `0x5BA057`
is `WinMainCRTStartup`, and `0x5BA15F` is the return from `_exit` inside
its `__except` block (`0x5BA154`: `mov esp, [ebp-0x18]; push [ebp-0x68];
call _exit`) - **an exception nobody handled, which the CRT turns into
`_exit(exception code)` with no dialog**. Not an `exit` in
`Display_Setup`'s failure paths, and no CRASH line because the crash
reporter skips breakpoints and single steps (signals, not faults).
`BOF3X_EXITTRACE` now also logs `ExitProcess`'s code and any breakpoint or
single step that every vectored handler passed on:

    exittrace   exception 0x80000004 at 0x005A9A3C, esp 0x000EFC9C, eflags 0x00200202, ...
    exittrace   the exception from 0x5AA6D8 0x5A618C 0x590AEE 0x5A5861 ...
    exittrace   ExitProcess(0x80000004)

`0x5A9A30`, an armed entry since the list began, is the MMX probe:
`pushfd; pop eax; mov edx, eax; xor eax, 0x200000; push eax; popfd;
pushfd; ...; cpuid; test edx, 0x800000`. The tracer steps each armed
entry's first instruction with the trap flag set, so that `pushfd` saves
TF = 1, the `popfd` puts it back, and the step after the next instruction
(`0x5A9A3C`) belongs to no pending entry - passed on, unhandled, `_exit`.
It is reached only on **the software renderer's** set-up: `Display_Setup`
`0x5A5861` → `0x5A60E0`, whose `Gfx_RenderFlags` bit 0 branch calls
`0x5AA671` (the pixel-conversion set-up) and it the probe. `ab27_orig`'s
counts have `0x5A60E0` with `0x5A62C0` from `0x5A61AB` - the hardware
branch - and no `0x5AA671`: the references before ran `renderer=1`. The
owner's `bof3x.ini` and the `wm1` scratch copy said `renderer=0` - which
also settles the launcher's "which value is which": 0 is Capcom's software
renderer, 1 the HAL ([`launcher-settings.md`](launcher-settings.md) §5).
Measured both ways the same evening, traced all-original, 40 s each:
`renderer=1` 618 logic frames; `renderer=0` exit at 15.9 s (and once a
hang at 0.17 s of CPU with nothing in the log - seen once, not chased).

The other session's "our WinMain kept, still exits" run is explained too:
our WinMain called our `Display_Setup` directly, so it did not run
Capcom's - but its log has no `DIV-0035` lines either, so it was not this
source's build; not reproducible from what is left.

**Fixed in `src/hook/calltrace.cpp`**: when the step it asked for lands and
the stepped entry's first byte is `pushfd` (`0x9C`), the dword it pushed
gets TF cleared - the flags an untraced run saves - and a single step no
entry asked for is now a `Fatal` naming its address instead of a silent
CRT exit. `renderer=0` traced all-original: 723 logic frames in 40 s, the
probe counted once. Only one listed entry starts with `pushfd` (a scan of
`entries_logic.txt`'s 3,032 first bytes).

**The reference, `wm1b`** (`analysis/validate_wm1b.sh`, log
`analysis/attract/wm1b_batch.log`): from a scratch launcher whose
`bof3x.ini` is windowed with `renderer=1`, like every reference before;
reference sides at `--original "*,-Game_Clock"` with the foreground held,
the ours side `--no-front`. Results, 2026-09-23 15:00-15:18:

| Check | Result |
|---|---|
| `wm1b_orig`, `wm1b_origb` | 10,311 logic frames each in 6 minutes |
| orig vs origb | **identical on all 10,313 frames** |
| `wm1b_ours` (unfocused) | 10,326 logic frames |
| orig vs ours | 1 of 10,313 differs: **frame 0**, the set-up (442 calls against 290) - different since `rb1` made `Display_Setup` ours (`ab27_orig` against `rb1_ours`: frame 0 only, 449 against 296); every logic frame identical |
| orig vs `wm1_ours` (the afternoon's run) | 263 frames from 3418 on differ, **every one equal to the reference three frames later**: that run fell three logic frames behind at 3418, next to the attract sequence's music change at 3439, and stayed there. A timing-dependent wait, seen once, not in `wm1b_ours`; not chased |
| `ab27_orig` vs orig | every frame differs with equal counts: the WinMain range and the list changed the hash's content, as expected |

**The reference is `analysis/calltrace/wm1b_orig`** (twin `wm1b_origb`).
The DLL the batch ran predates the evening's DIV-0036 k choice, the
present's fit-down and `BOF3X_PIXEL_OFFSET`; at the batch's windowed k = 2
none of them changes a draw call or a logic frame, and the self-tests pass
on the final build (0 mismatches, 796 ours).

Also seen: the first windowed run (before the batch) was started from a
background shell and never given the foreground; it ran to the attract
demo's `DEMO.DAT` in 50 seconds at 33 seconds of CPU - DIV-0033 in effect
from the first frame.

Not reached by any of this, owed to the owner (§5): the borderless window
on their monitor, F8 both ways, a resize of the windowed window, the FMVs
at three times, the pause lines (F9), the pads reading zero while another
window is in front, and DIV-0034 after a title-bar drag.

## 5. Open

- **Owner's eye, done 2026-09-23 evening:** fullscreen (the borderless
  window) and F8 work; a 3x window from the launcher looks right; resizing
  is allowed and breaks nothing; the FMVs play at 2x and 4x; the game keeps
  running when not in front and keys typed elsewhere do not reach it; F9's
  pause shows (in Chinese - §6, now English). The title-bar drag (DIV-0034's
  log line) is the one left, and matters only under `BOF3X_BACKGROUND=0`.
- The overlays are logged, not drawn (DIV-0032). Drawing them through the
  backend - a small text pass, or `Text_DrawAt` with the UI font once the
  strings are mapped to its glyphs - is a later nicety; F12's "Save OK" is
  the one a player would miss.
- `Fmv_Play` under `BOF3X_ORIGINAL=Fmv_Play` on a borderless window is
  untested: Capcom's player would mode-set 640 x 480 over it. **Reachable
  only since the review** (2026-09-23 evening): our WinMain called
  `Fmv_Play`, `Display_Setup` and registered `Game_WndProc` directly, so
  those three names' `BOF3X_ORIGINAL` did nothing under our WinMain -
  including DIV-0031's documented way back to Capcom's DirectDraw. It
  reaches them through Capcom's addresses now; `BOF3X_ORIGINAL=Display_Setup`
  under our WinMain ran Capcom's set-up and entered the loop (checked live).
- Also from the review: a quit during the FMVs (Alt+F4, which
  `Fmv_WndProc` turns into the quit flag) returns at once, as `0x4FCD3A`
  does; ours had torn down a display it never set up.
- The window title is still the GBK string `0x65DA78`; an English title
  under `BOF3X_LANG=en` would be a one-line DIV.
- `tools/input_run.py` still foregrounds the window for its captures
  (they want it unobscured, not focused); it could take `--no-front` too.
- Step 4 of the overhaul (integer scaling, `k` from the client) is next:
  `BOF3X_SCALE` still rasterises at 2 and the present scales that by an
  integer; a 1440-row monitor wants the target at 3 (§4b's `sprt_draw.cpp`
  far-edge table first).

## 6. The F9 pause in English (DIV-0038)

The owner asked what F9's Chinese screen says (2026-09-23). The two lines
WinMain draws while `Game_Paused` are `Pause_LinesGame` `0x66A418` in game
and `Pause_LinesTitle` `0x66A448` while the title's flags are set - which
includes the attract sequence. They are text-code strings in the exe's
`.data`, drawn by `Text_DrawAt` from the glyph table, not images; rendering
their codes from `FIRST.DAT`'s table matched the owner's screenshot glyph
for glyph:

| Pointer | String | Chinese | Meaning |
|---|---|---|---|
| `0x66A418` | `0x66A3F0` | 再按一次F9回主画面 | press F9 again: back to the title screen |
| `0x66A41C` | `0x66A404` | 按其他键继续游戏 | press any other key: go on playing |
| `0x66A448` | `0x66A420` | 再按一次F9结束游戏 | press F9 again: quit the game |
| `0x66A44C` | `0x66A434` | 按其他键回主画面 | press any other key: back to the title screen |

- which is what WndProc does with the next key. The disc text never
reached them because they are on no disc. `src/game/pause_text.cpp`
re-aims the four pointers at English of ours once an English overlay's
advance table is installed (`dat_load.cpp`, chunk kind 4), through
`PatchBytes` as `PauseText`; our WinMain centres a line of ours on its real
width (`PauseText_X`). The English is plain ASCII in the overlay's encoding.
A first wording of 42 characters ran off both edges (about 8 units a
character against 320); every line is now 31 to 35 characters:

- in game: "Press F9 again for the title screen" / "Press any other key to continue"
- on the title: "Press F9 again to quit the game" / "Any other key returns to the title"

Checked 2026-09-23 at k = 3, English, F9 posted to the window
(`analysis/shots/pause2/title.png` in the attract sequence,
`analysis/shots/pause3/field.png` in save 5's field): both pairs centred and
inside the screen. Self-tests 0 mismatches. The other function keys: F7
cycled the device name and F11 the frame rate - both nothing since DIV-0040
(the owner: not needed); F12 writes an ordinary save to slot 0
(`BISLPS00.DAT`, [`save-files.md`](save-files.md)) with "Save OK", logged
not drawn (DIV-0032); F10 and Alt are swallowed. The window's GBK title,
mojibake outside a Chinese locale, is "Breath of Fire III" (DIV-0039).
