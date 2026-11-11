# Linux: staging for two tiers (issue #215, IDEAS I62)

**Status:** IN PROGRESS (staged 2026-11-10 at `main` `488a5594`. **Tier 1's
first pass was run the same day** under UMU-Proton 10.0-4 in this machine's
WSL2, section 2a: the importer sets up a disc-only install, the 32-bit build
loads a save and plays a battle, and three defects were found, D411..D413.
**D411 and D412 are fixed on `fix/proton-d411-d412`**, section 2b: both
widths then play the same route under Proton with nothing added by hand.
The launcher's pages, a real GPU, the pad, the sound by ear and a Steam Deck
are not tested. Tier 2 is staged and waits for the owner's word and the calls
in section 6.)

Issue #215 asks for the game on Linux and the Steam Deck. This file stages
two answers, the cheap one first:

- **Tier 1 - supported under Proton / Wine.** The Windows release, run
  through a compatibility layer, tested and documented. No new build.
- **Tier 2 - a native Linux build of the engine.** An ELF of the 64-bit
  host. The launcher stays a Windows program at first (section 5 names what
  stands in for it); a native launcher is tier 3 and is not staged here.

Every count below was measured on `488a5594` by the command beside it. What
was not measured is in section 7.

## 1. What is known today

**One observation on Linux, and it is not ours.** The screenshot in issue
#215 shows a self-built `bof3x.exe` started under UMU-Proton 10.0-4. The
launcher starts and shows this project's own DIV-0102 message: a disc-only
install with no `BOF3.exe` in the game directory and no language layer in
the cache. That is an install not yet set up, reported correctly. So a Win32
dialog of ours draws under Proton, and nothing past the launcher has been
seen. Whether the Sources page, the importer, the renderer, the sound or the
movies work there is unknown.

**What the engine is, for this purpose.** All of the game's code is ours
(10,088 functions in the exe host, [`STATUS.md`](STATUS.md)). The 64-bit host
runs no code of Capcom's and never could
([`x64-build-round.md`](x64-build-round.md) section 9). The 32-bit build is
the player's default and the 64-bit build is a second release artefact since
`v0.1.0-beta.1`.

**What is already portable:**

| Piece | State |
|---|---|
| Sound output | SDL3 audio since DIV-0201 (`src/audio/sound_sdl.cpp`) |
| Pad | SDL3 (`src/input/pad_sdl.cpp`) |
| Movies | Our own Cinepak and Video 1 decoders (`src/video`, DIV-0205); no Windows header in the folder. The picture is drawn by GDI (`StretchDIBits`, 2 sites) |
| MP3 | minimp3, vendored (`third_party/minimp3`) |
| x87 asm | All 127 x87 and control-word blocks assemble for x86-64 ([`x64-build-round.md`](x64-build-round.md) XA3) |
| CI | Both widths already build on `ubuntu-24.04`, as a cross-compile with llvm-mingw (`.github/workflows/build.yml`) |
| The importer | Python (`tools/importer.py`); the release bundles CPython for Windows |

**What is Windows.** Files that include a Windows header, by folder
(`grep -rlE '#include *<(windows|d3d|ddraw|dsound|dinput|xinput|mmsystem|shlobj|commctrl|vfw|winsock)' src/<folder>`):

| Folder | Files | With a Windows header |
|---|---|---|
| `src/game` | 2,090 | 88 (field 24, platform 23, menu 8, battle 8, text 7, core 6, effect 4, the root 4, shop 3, minigame 1) |
| `src/hook` | 49 | 21 |
| `src/launcher` | 15 | 10 |
| `src/render` | 10 | 6 |
| `src/common` | 9 | 3 |
| `src/audio` | 10 | 2 |
| `src/input` | 8 | 2 |
| `src/video` | 9 | 0 |

The headers named (`grep -rhoE` over `src`): `windows.h` 131, `dinput.h` 9,
`d3d11.h` 2, `d3dcompiler.h` 2, `shlobj.h` 2, `mmsystem.h` 2, `commctrl.h` 2,
`shellapi.h`, `dbghelp.h` and `commdlg.h` 1 each; `SDL3/SDL.h` 3. The DLL
links `d3d11 dxgi d3dcompiler_47 uuid winmm bcrypt ole32 windowscodecs dinput
dxguid` and SDL3 static (`CMakeLists.txt:193`).

Win32 calls by name, the ones a port has to answer (a `grep -rhoE` census of
`src`, `.cpp` and `.h`):

| Group | Calls (sites) |
|---|---|
| Memory | `VirtualProtect` 38, `VirtualAlloc` 31 |
| Files | `CreateFileW` 26, `ReadFile` 19, `WriteFile` 13, `FindFirstFileA` 9, `CreateFileA` 8, `FindFirstFileW` 6, `CreateDirectoryA/W` 2 each |
| Clocks | `GetTickCount` 21, `QueryPerformanceCounter` 15, `Sleep` 3 |
| Modules | `GetModuleHandleW` 15, `LoadLibraryW` 7, `GetModuleHandleA` 4, `LoadLibraryA` 3 |
| Threads and processes | `CreateThread` 7, `CreateProcessW` 6 (the launcher and `exe_host.cpp`) |
| Faults | `AddVectoredExceptionHandler` 6, `SetUnhandledExceptionFilter` 2, `MiniDumpWriteDump` 2; `__try` in `win_main.cpp`, `calltrace.cpp`, `exe_host.cpp` |
| Fibers | `SwitchToFiber` 4 (`render_d3d11.cpp`: every entry into Direct3D runs on a fiber with a 1 MB stack; `snapshot.cpp`) |
| Dialogs | `MessageBoxA` 9, `MessageBoxW` 7 |
| Hash | the six `BCrypt*` calls, 2 sites each |
| Window | `CreateWindowExA` 1, `PeekMessageA` 2, `GetAsyncKeyState` 1 |
| Launcher sound | the six `waveOut*` calls, 1 site each, all in the launcher's Jukebox (`src/launcher/music_player.cpp`) |

Two more facts that shape tier 2:

- **`__stdcall` appears 356 times** (`grep -rnoE` over `src`). It means
  nothing on x86-64, so it is noise there and not a blocker; the asm written
  for the Win64 calling convention is the real item (section 4, LA3).
- **The narrow image needs low addresses.** The 64-bit build keeps the
  game's words 32 bits wide, so the data image must sit below 4 GB, and
  `bof3x.exe` is itself linked at `0x400000`
  ([`x64-build-design.md`](x64-build-design.md)). Linux lets a program map
  down to `mmap_min_addr`, 64 KB by default
  ([`x64-build-round.md`](x64-build-round.md) section 1, stated there and
  not probed).

## 2. Tier 1: supported under Proton / Wine

**Goal:** a player on Linux or a Steam Deck installs the Windows zip, sets up
an install, and plays, by steps the README states. Each failure found is
either fixed in the Windows build or written down as a known limit.

**Who runs it.** This machine has no Linux. The pass needs a tester with one;
the screenshot in #215 came from such a machine. The project supplies the
checklist below and reads the `bof3x.log` that comes back.

**The checklist, in the order a player meets it.** Each row is run on the
signed release zip, both widths, and reported with the Proton or Wine
version, the GPU and driver, and `bof3x.log`.

| # | Step | What can go wrong there |
|---|---|---|
| 1 | The launcher starts; its three pages draw | Common controls, the manifest, fonts for CJK labels |
| 2 | Sources: pick a disc image through the file dialog | `commdlg` paths: a Linux path shows as `Z:\...`; a case-sensitive folder |
| 3 | Sources: the install runs to the end | The bundled CPython 3.13 with numpy, Pillow and capstone under Wine; `CreateProcessW` with pipes for `--json-progress`; the time it takes |
| 4 | The game window opens and draws | Direct3D 11 through DXVK; `D3DCompile` at run time goes to Wine's own HLSL compiler, which miscompiles the scene shader (D411, section 2a) |
| 5 | Window modes, 21:9, the point filter | The borderless and fullscreen paths on a compositor; the Deck's 1280 x 800 |
| 6 | Sound: music, effects, a battle voice | SDL3's Windows backend inside Wine; latency |
| 7 | Pad: bindings, hotplug, the Deck's built-in controls | SDL3's pad path against Steam Input; DirectInput (`d_input.cpp`) as the second path |
| 8 | The Capcom movie and the opening | The GDI blit on the wall clock |
| 9 | Save, load, quicksave (F6 / F7), quit and start again | The home folder's place (`shlobj`), file locking |
| 10 | One hour of play, a battle, a town, the world map | Anything timing-shaped: the 32-bit build under WoW64 on a 64-bit-only Wine |
| 11 | A crash, if one happens: the `CRASH n:` block and the dump file are written (no switch forces one; [`crash-reporter.md`](crash-reporter.md)) | The vectored handler and `MiniDumpWriteDump` under Wine |
| 12 | Added as a non-Steam game on a Deck, Game Mode | Launch through the launcher; the launcher starting the engine as a second process |

**What comes out:**

1. A section in the top-level README, in Simplified Technical English: the
   supported way to run on Linux, the Proton version tested, the known
   limits.
2. A line in the bug-report template for the compatibility layer and its
   version.
3. Defects, each with a number in `known-defects.md`; a fix that changes
   behaviour on Windows gets a ledger entry like any other.
4. A decision on which width to recommend there. The 64-bit build avoids
   WoW64; the 32-bit build is the one players have finished the game on.

**Cost and risk.** No code is planned; the cost is the tester's time and the
fixes the list finds. The likeliest fixes are small (a path, a DLL to name in
the notes). The importer under Wine (row 3) is the largest unknown.

**Done means:** rows 1 to 10 pass on one desktop distribution and one Steam
Deck, or each failing row has a defect number and a stated limit.

## 2a. Tier 1's first pass, as run (2026-11-10)

**Where.** This machine's WSL2 (Ubuntu 26.04, kernel 6.18.58.1, WSLg),
umu-launcher 1.4.4 as a zipapp in the home folder, UMU-Proton 10.0-4 and
Valve's `steamrt3` runtime as umu fetched them - the stack of issue #215's
screenshot. The two signed zips of `v0.1.0-beta.1`, unpacked on the Linux
filesystem. Root `D:\bof3x-runs\linux1110` (scripts, logs, shots; the
installs themselves under `~/linux1110` in the WSL, game-derived). **The
limits of this place:** WSL has no Vulkan driver for the GPU, so DXVK ran on
llvmpipe (software, about 22 frames a second at 3440 x 1440); it has no 32-bit
graphics libraries, so the 32-bit build needed `PROTON_USE_WOW64=1`; nothing
here was clicked, heard or held in the hand. The game was started by
`bof3x-launcher.exe --no-config` with a recipe (`BOF3X_INPUT`) and wrote its
own frames (`BOF3X_SHOT_DIR`).

| Row | Result | Evidence |
|---|---|---|
| 1 Launcher starts | Not run here. Issue #215's screenshot shows a dialog of it drawn | - |
| 2 The file dialog | Not tested | - |
| 3 The install | **Passes for a PlayStation disc, both widths.** By the launcher's contract run by hand: `identify --json` names the US disc and the PSP Europe image; `sources add` writes `sources.toml`; `setup --json-progress` builds a disc-only install from the US disc in 122 s (64-bit zip) and 126 s (32-bit zip), `done game=... cache=...`. Disc paths are `Z:\mnt\...` | `imp_setup.out`, `imp_setup_x86.out`, `setup.log` |
| 3 with a PSP disc | Stops as on Windows without ffmpeg: `error --opt psp-snd: no ffmpeg ...`. What a Linux player gives as `ffmpeg.exe` is not tested | `imp_setuppsp_x86.out` |
| 3, numpy | `import numpy` in the bundled CPython aborts: `unimplemented function ucrtbase.dll.crealf` (D413). The set-up of a PlayStation disc did not reach it; Pillow and capstone import | `py.sh` |
| 4 Draws, 32-bit | **Draws, with wrong colours** unless Microsoft's `d3dcompiler_47.dll` is beside the exe with `WINEDLLOVERRIDES=d3dcompiler_47=n,b` (D411). With it the title, the attract scenes and a battle are right | `shots_boot86w` (wrong), `shots_boot86n`, `shots_combat86s` (right) |
| 4 Draws, 64-bit | **Stops on frame 1** with our own `FATAL`: a pointer above 4 GB (D412). Direct3D, the pad and the sound were up before it | `bof3x_boot64b.log` |
| 5 Window modes | The default borderless window at the desktop's 3440 x 1440, scale 6. Other modes not tested. Once, on the first window the new prefix ever made, the desktop read as 1024 x 768, the client as 0 x 0 and `D3D11CreateDeviceAndSwapChain` failed (`0x887A0001`); the starts after it were right | `bof3x_boot64.log` |
| 6 Sound | SDL3 opens `PulseAudio Output` at 44,100 Hz and the songs load from the cache. Not heard | `bof3x_boot86w.log` |
| 7 Pad | `pad: SDL 3.4.16, gamepad subsystem up`. No pad attached | the same |
| 8 Movies | A disc-only install without ffmpeg has no logo movie, so `Fmv_Play` drew nothing. Not tested | - |
| 9 Save and load | A recipe save in slot 0 loads and the recorded `combat` route (2,620 frames, en-US) plays to its end: the ability window, a failed steal, damage numbers. Quicksave not tested | `shots_combat86s`, `bof3x_combat86s.log` |
| 10 An hour of play | Not done: 2,640 frames of attract and 2,620 of the route | - |
| 11, 12 | Not tested | - |

**The three defects** ([`known-defects.md`](known-defects.md)):

- **D411, every Proton player, both widths.** Wine's built-in
  `d3dcompiler_47` compiles `kSceneShader` so that each vertex's colour reads
  as its clip-space position: the scene under a screen-wide gradient, green
  at the top, blue at the bottom left, red at the bottom right; the text
  windows stay white. Microsoft's DLL beside the exe gives the right
  picture, the one change between the two runs. The fix that needs nothing
  of the player is to stop compiling at run time: carry the compiled
  bytecode in the build.
- **D412, the 64-bit build under Wine.** `Crt_fopen` returns the C
  runtime's `FILE*` and `file.cpp` stores it in a game word. On Windows the
  object is on the heap, below 4 GB; in Wine's `ucrtbase.dll` it is a static
  object inside the DLL, which Wine loads at `0x6FFFFF9E0000` (`+loaddll`).
  `Low32()` stops the game by name, as the 64-bit design meant it to
  ([`x64-build-design.md`](x64-build-design.md)). The design's premise - the
  system places its objects below 4 GB when ASLR is off - is Windows'
  behaviour and not Wine's, so this may not be the only such pointer; it is
  the first one met. The 32-bit build is the one to recommend on Linux until
  this is settled.
- **D413, the bundled Python.** The import of numpy fails under Wine 10
  (`crealf`). No step of a PlayStation set-up imports it; `at3.py` does,
  inside three functions.

**What the pass settles.** The 32-bit release plays under Proton today with
one DLL added by hand. Rows 1, 2, 5 to 8 and 10 to 12 still want a Linux
machine with a GPU, and a person.

## 2b. The two fixes (2026-11-10, branch `fix/proton-d411-d412`)

**D411: the build carries the shaders' bytecode.** `tools/compile_shaders.py`
reads the three HLSL sources where they are (the raw strings of
`render_d3d11.cpp` and `satpixie.cpp`), compiles the ten shaders with Windows'
`d3dcompiler_47.dll` by the arguments the renderer passed, and writes
`src/render/shaders_compiled.h` (28,892 bytes of bytecode, committed; two runs
of the tool give the same file). The renderer creates its shaders from that
and calls `D3DCompile` only when `BOF3X_PIXEL_OFFSET` asks for another offset.
A stale header stops by name: the game checks each source's sha256 at
start-up and CI runs `compile_shaders.py --check`.

**D412: a stream above 4 GB is a slot of ours.** `src/common/file_token.h`:
on x86-64, `Crt_fopen` gives the game the `FILE*` itself when it lies below
4 GB (Windows: the words hold what they held) and otherwise the address of a
slot in a 64-entry table in `bof3x.dll`; the other `Crt_f*` functions turn it
back. The 32-bit build's three functions are inline identities.

**Proof.**

| Run | Result | Evidence (`D:\bof3x-runs\linux1110`) |
|---|---|---|
| Proton, 32-bit build of the branch, no native DLL, the `combat` route with 7 shots | The 7 frames are byte-identical to the release's frames under Microsoft's compiler | `shots_fix86` against `shots_combat86s` |
| Proton, 64-bit build of the branch, the same route and save | Plays to the end; the log names the table once (`file: the C runtime's FILE 00006FFFFFA6BA28 ...`); the 7 frames are pixel-identical to the 32-bit run's. No second pointer above 4 GB met on this route or on the opening's first 2,620 frames | `shots_fix64b`, `shots_fix64`, `bof3x_fix64b.log` |
| Windows, 32-bit, the boot recipe (8 shots, 2 of them black fades): the build's bytecode against `BOF3X_PIXEL_OFFSET=0.5`, which compiles at start-up as before | 8 of 8 frames identical | `C:\Users\kerok\bof3x-runs\linux1110\shots_w32a`, `shots_w32b` |
| Windows, 64-bit, the same recipe | 8 of 8 identical to the 32-bit frames | `shots_w64a` |
| Windows, 32-bit, `BOF3X_PRESENT=satpixie` | Starts and runs the recipe; the filter's five shaders are created from the bytecode | `shots_w32sat` |

`width_census.tsv` is regenerated for `file_token.cpp`'s sites; the data,
relocation, bare-address, unit and shell checks pass unchanged. **Not run:**
the ten state-hash routes and `codegen_same.py` (the change to the 32-bit
build is the renderer's start-up and three inline identities; the frames
above are the check made). **No ledger entry:** nothing a player on Windows
sees changes - the same compiler's output for the same source, made at build
time. If the owner reads it as a divergence it takes the next number.

## 2c. The second pass, on a Linux machine with a GPU (2026-11-11)

**Where.** CachyOS, KDE on Wayland, an RTX 5090, Proton Experimental
(prefix 11.0-100) through umu-launcher 1.4.3; `main` `8d2d987d`'s two
release zips, built on this machine by `tools/make_release.py` with the
pinned llvm-mingw 20260616 (both widths build on Linux unchanged). Commands
run through a `.bat` under Proton's `cmd`, since umu does not pass a
console's output through.

| Row | Result |
|---|---|
| 3 The install | **Passes, 32-bit zip:** `identify --json` and `sources add` name the US disc; `setup --json-progress` builds a disc-only install in 62 s; no logo movie for want of ffmpeg (the PSX media round, [`psx-media-round.md`](psx-media-round.md), removes that need). `verify`'s one problem is `opt/area4-walls`, known on a US-only cache (HANDOFF) |
| 4 Draws | **Passes, both widths**, with nothing added to the prefix (no native `d3dcompiler_47`, no `PROTON_USE_WOW64`, no override). Runs fail at start while the NVIDIA kernel driver refuses new GPU channels; native `vulkaninfo` fails too (D414, [`known-defects.md`](known-defects.md)) |
| 6 Sound | **Passes:** the music, by the owner's ear; the game's stream reaches the default PipeWire sink at 100% |
| 8 Movies | **Plays** from `CAPCOM30.STR` with the PSX media round's branch (DIV-0214); typing in another window can land in the game's window, which takes the focus, and skips it |

Rows 5, 7 and 9 to 12 are not yet run here.

## 3. Tier 2: a native build - the shape

**Goal:** `bof3x` as an x86-64 Linux ELF that plays an install made
elsewhere or by the command-line importer, proven against the Windows 64-bit
build by the state hash.

**Target:** x86-64 Linux alone. ARM64 needs the software x87 model the
64-bit round put off; macOS shares the platform seam and adds its own
low-memory question. Neither is in this round.

**The method is the 64-bit round's:** one source, the existing build the
oracle. Every change lands in the Windows builds first and is judged by the
gates that exist (`codegen_same.py` where the code must not change, the ten
state-hash routes, `music_heard.py`). The Linux build is linked only after
the source builds for it, and is then compared with the Windows 64-bit build
route by route.

**The seam.** Today the Win32 calls are spread through `src/hook`,
`src/render`, `src/common`, `src/game/platform` and 65 game files outside
`platform`. The round's first job is to say what each of those 65 files
wants from `windows.h` (a type, a clock, a file, or nothing) and to move
every real need behind one small interface with two implementations. The
game folders then include no operating-system header at all, and CI holds
that with a ratchet as the typed-data round held its census.

## 4. Tier 2: the stages

### Stage LA - measure and design (no behaviour change, no Linux build)

- **LA1, the census.** A tool beside `width_census.py` that lists, per file,
  every Windows header, type and call, and classes each site: memory, file,
  clock, thread, fault, window, dialog, graphics, input, sound, launcher,
  harness-only. Its output is stage LB's list. It also answers what the 65
  game files outside `platform` use.
- **LA2, the low-memory design on Linux.** Probes on a Linux machine, as
  XA2's were on Windows: an ELF that maps the data image at its addresses;
  where the ELF itself may sit (a non-PIE ELF's default base is also
  `0x400000`, so either the link places our code elsewhere or the image
  loads at a moved base, which the relocatable-image round already runs);
  `mmap_min_addr` on the common distributions and the Deck; the task stacks
  below 4 GB.
- **LA3, the asm and the calling convention.** Which of the 174 asm blocks
  name a Win64 register rule (the stack switch saves `rbx`, `rbp`, `rsi`,
  `rdi`, `r12`..`r15` and `xmm6`..`xmm15`; System V's callee-saved set is
  different and has no callee-saved `xmm`). Two roads, settled by a build:
  mark the affected functions `ms_abi` and keep one asm text, or write a
  System V variant of the few blocks that switch stacks. Clang for an ELF
  target is the proposed compiler, so the asm dialect stays the one in use.
- **LA4, the renderer's interface.** What `render_shim.cpp` (1,347 lines)
  asks of `render_d3d11.cpp` (1,159 lines), written down as the interface a
  second backend fills; the shaders listed (they are HLSL source compiled at
  run time by `D3DCompile`); the readback paths (`fb_grab.cpp`,
  `gfx_snapshot.cpp`) the harness's shots depend on. `src/game/platform/d3d.cpp`
  (4,042 lines) is read for which side of the interface it is on.
- **LA5, the paths.** Every file name the game and the importer build,
  checked for case against what an install holds, since a Linux filesystem
  distinguishes `DAT\AREA000.EMI` from `dat/area000.emi`. The home folder's
  Linux place (XDG).

**Gate:** documents and a tool; the Windows objects unchanged.

### Stage LB - make the source portable, on Windows

Each unit lands on the Windows builds and passes the existing gates before
the next.

| Unit | Change | Proof on Windows |
|---|---|---|
| LB1 | The operating-system seam: memory, files, directory walks, clocks, threads, the hash (BCrypt -> `src/common/sha256.cpp`, which exists), message boxes -> SDL's | `codegen_same.py` where only spelling changed; the ten routes |
| LB2 | The window and the message loop on SDL3 (`win_main.cpp`, `display_setup.cpp`); the keyboard on SDL3 | The ten routes; the owner's eye on the window modes |
| LB3 | DirectInput retired or kept as a Windows-only second path (call 4); `sigma.cpp` and `bindings.cpp` checked | `button_tests` |
| LB4 | The movie picture drawn through the renderer in place of GDI | The three movies' frames against the decoder's own dump |
| LB5 | The second renderer backend behind LA4's interface (call 1), run on Windows beside Direct3D 11 | Shots of the ten routes compared between the two backends, pixel for pixel where the filter is point |
| LB6 | The fibers replaced: the render call's own stack by the task switch already ours, or by a thread; `snapshot.cpp` likewise | The ten routes; `stackwatch` |
| LB7 | Faults: the crash reporter split into a portable report (`CRASH n:` lines) and a per-system catcher; the harness-only watchers (`write_watch`, `gapwatch`, `calltrace`) marked Windows-only if they stay so | A crash on purpose, read by `tools/crash_report.py` |
| LB8 | The case of every path fixed at its source (LA5) | A Windows run with a case-checking file open in the seam |

**Gate:** the Windows builds play as before; `BOF3X_OFF` switches for
anything a player could tell apart (the second backend, the SDL window),
each with a ledger entry if it is one.

### Stage LC - link and run on Linux

- A CMake preset for the ELF, clang, SDL3 from source as today.
- The exe host's start-up without a PE loader: the data image read and
  mapped by our own code (`exe_image.cpp` already builds an image from a
  disc-built install).
- The harness on Linux: `input_run.py` and `abc_run.py` drive a Windows
  process today; six tools use `ctypes` or `winreg`
  (`grep -rlE "import (ctypes|winreg|win32|msvcrt)|ctypes\.windll" tools`).
  The minimum is recipes replayed by `BOF3X_` environment switches and the
  state hash written to a file, which needs no window control.
- An install: made on Windows and copied, or made by `tools/importer.py`
  under the system's Python (call 3).

**Gate:** the ten routes on Linux against the Windows 64-bit references,
with the same allowance the 64-bit round used (pages on which two runs of
one build also differ); `music_heard.py` passes; a quicksave made on Linux
loads on Linux.

### Stage LD - audit, CI, release

- A native `ubuntu` CI job as a gate, and the header ratchet of section 3.
- The release artefact (call 5), its notes, the README section.
- A play by a Linux tester of the tier-1 checklist's rows 4 to 10 on the
  native build, and the owner's eye on the second backend on Windows.
- Rewrites of [`STATUS.md`](STATUS.md) and [`HANDOFF.md`](HANDOFF.md).

## 5. What stands in for the launcher in tier 2

The launcher is Win32 dialogs: `config_dialog.cpp` 1,861 lines, `config.cpp`
1,485, `sources_dialog.cpp` 1,190, `music_player.cpp` 927, `launcher.cpp`
691, `suites.cpp` 281, `exe_image.cpp` 68 - 6,503 in all (`wc -l`). Tier 2
does not rewrite it. Three stand-ins, cheapest first:

1. **The Windows launcher under Wine writes the configuration; the native
   engine reads it.** Possible only if both agree on where the home folder
   is; LA5 settles that.
2. **A text configuration and the importer's command line.** The `.ini` the
   launcher writes is already the contract; the importer's `identify`,
   `sources` and `setup` calls are already a command line
   ([`install-sources.md`](install-sources.md) section 4).
3. **Tier 3:** the three pages in a portable toolkit. IDEAS I40 (the
   importer in C++) shrinks it, since the Sources page is then no longer a
   front end to a Python process.

## 6. For the owner

1. **The second renderer's API.** Proposed: SDL3's GPU API, since SDL3 is
   already built in and it reaches Vulkan on Linux and Direct3D 12 or Vulkan
   on Windows from one backend. The alternative is OpenGL 3.3, which is
   simpler and reaches older hardware. Either needs the shaders in a second
   form; how is LA4's finding.
2. **Whether Direct3D 11 stays on Windows** once the second backend passes.
   Proposed: yes, as the default there for a release, with the new backend a
   switch; then the owner's call.
3. **The importer on Linux.** The system's Python with a requirements file
   (proposed, for a first native release), or wait for I40.
4. **DirectInput.** Retire it for SDL3 on both systems (proposed, one input
   path to test), or keep it as a Windows-only path.
5. **The release's form.** A plain tarball (proposed for the first), an
   AppImage, or a Flatpak. The Deck is served by any of them as a non-Steam
   game.
6. **The order.** Proposed: tier 1 now; tier 2 after the renderer's
   remainder ([`platform-layers-plan.md`](platform-layers-plan.md) 2.2),
   because LB5 writes against whatever that round leaves.

## 7. Not verified

- Tier 1 on a real GPU, with a person: section 2a ran on a software
  renderer and names each row it left.
- That D411 shows on a hardware Vulkan driver as it does on llvmpipe. The
  compiler is the same on both, and the native DLL alone cured it, so it
  should; it was not seen.
- That the 32-bit build runs without `PROTON_USE_WOW64=1` where 32-bit
  graphics drivers are installed.
- `mmap_min_addr` read 65,536 in this machine's WSL2 (`/proc/sys/vm/mmap_min_addr`,
  2026-11-10). Not read on the other distributions players use or on
  SteamOS; that an ELF of ours can keep `0x400000`..`0x98F000` free or
  occupy it as the PE does.
- What the 65 game files outside `platform` use from `windows.h`. The one
  file sampled (`src/game/field/area_map.cpp`) showed only the project's own
  accessors; that is one file.
- Which side of the renderer's interface `src/game/platform/d3d.cpp` is on.
- Whether a switch exists that forces a crash for the checklist's row 11
  (a search of `src` for `CRASHTEST` found none; the reporter's doc was not
  read for another name).
- That SDL3's GPU API covers what `render_d3d11.cpp` does (the present pass,
  the readbacks, the texture page uploads). LA4 reads it against SDL's
  documentation before call 1 is made.
- The count of asm blocks that depend on the Win64 register rule; LA3
  measures it.

## 8. Not in these tiers

- A native launcher (tier 3) and the importer in C++ (I40).
- ARM64 and macOS.
- The 32-bit build on Linux. The native build is the 64-bit host; the 32-bit
  build reaches Linux through tier 1.
- Any change to what the game does. A native build adds no behaviour; what
  stage LB changes on Windows is ledgered there.
