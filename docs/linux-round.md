# Linux: staging for two tiers (issue #215, IDEAS I62)

**Status:** IN PROGRESS (staged 2026-11-10 at `main` `488a5594`. **Tier 1's
first pass was run the same day** under UMU-Proton 10.0-4 in this machine's
WSL2, section 2a: the importer sets up a disc-only install, the 32-bit build
loads a save and plays a battle, and three defects were found, D411..D413.
**D411 and D412 are fixed on `fix/proton-d411-d412`**, section 2b: both
widths then play the same route under Proton with nothing added by hand.
The launcher's pages, a real GPU, the pad, the sound by ear and a Steam Deck
are not tested. Tier 2 is staged and waits for the owner's word and the calls
in section 6. **2026-11-19:** stage LB is done and on `main` (PRs #259, #261,
#262, #264): every source file compiles for x86-64 Linux and nothing links.
Stages LC and LD are staged as one round of fourteen groups in three waves:
section 4, "Stages LC and LD, staged as one round". **Wave A is on `main`**
(PR #265, 2026-11-19). **2026-11-20:** the link measured natively in the
WSL is 7 names short; wave B is staged as four units: section 4, "Wave B,
staged". **2026-11-21:** wave B ran on the branch `linux/lcB` (not on
`main`, not pushed): the game links as a Linux ELF, starts through the Linux
launcher and ends the ten state-hash routes headless in the WSL, on a
software Vulkan device; the gate's Windows half ran the same evening and
passed. No picture was looked at on a screen and nothing ran on a GPU:
section 4, "Wave B as run".)

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

## 2c. The second pass, on frost's Linux machine with a GPU (2026-11-11)

**Where.** CachyOS, KDE on Wayland, an RTX 5090, Proton Experimental
(prefix 11.0-100) through umu-launcher 1.4.3; `main` `8d2d987d`'s two
release zips, built on that machine by `tools/make_release.py` with the
pinned llvm-mingw 20260616 (both widths build on Linux unchanged). Commands
run through a `.bat` under Proton's `cmd`, since umu does not pass a
console's output through.

| Row | Result |
|---|---|
| 3 The install | **Passes, 32-bit zip:** `identify --json` and `sources add` name the US disc; `setup --json-progress` builds a disc-only install in 62 s; no logo movie for want of ffmpeg (the PSX media round, [`psx-media-round.md`](psx-media-round.md), removes that need). `verify`'s one problem is `opt/area4-walls`, known on a US-only cache (HANDOFF) |
| 4 Draws | **Passes, both widths**, with nothing added to the prefix (no native `d3dcompiler_47`, no `PROTON_USE_WOW64`, no override). Runs fail at start while the NVIDIA kernel driver refuses new GPU channels; native `vulkaninfo` fails too (D414, [`known-defects.md`](known-defects.md)) |
| 6 Sound | **Passes:** the music, by frost's (raulcorreia7) ear; the game's stream reaches the default PipeWire sink at 100% |
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
- **LA3, the asm and the calling convention.** Which of the 328 asm statements
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

### Stage LA as run (2026-11-11)

Read-only: no file under `src/` changed. The tools are `tools/os_census.py`
(output `analysis/os_census.tsv`, checked by `python tools/os_census.py
--check`), and under `tools/linux_probe/`: `lowmem_probe.cpp`,
`msabi_probe.cpp`, `asm_by_abi.py`, `linux_check.py`. Machine: CachyOS,
kernel 7.3.1-1-cachyos, clang 23.1.4.

**LA1, the census.** `python tools/os_census.py --counts` gives 4,598 sites in
140 files: 1,643 calls, 1,456 constants, 1,325 types, 174 includes. By class:

| Class | Sites | Class | Sites |
|---|---:|---|---:|
| launcher | 1,811 | thread | 79 |
| harness-only | 696 | input | 49 |
| type_only | 502 | crt | 43 |
| file | 340 | fault | 39 |
| process | 298 | clock | 28 |
| graphics | 227 | hash | 19 |
| window | 162 | dialog | 17 |
| memory | 144 | fiber | 12 |
| umbrella (the `windows.h` row) | 130 | sound | 2 |

The launcher and the harness files are 2,507 of the sites; the other 2,091
are what a native build must provide or remove. A site's class follows the
file first (harness-only by name, launcher by folder), then the type rule, then
the API group. The calls, by the area that makes them, are in the TSV
(`--list CLASS`, `--files PREFIX`). The ones outside the launcher and the
harness files: `game/platform` 146, `hook` 304 (a Windows-only layer), `render`
52, `game` other 104, `audio` 34, `common` 25.

**The 65 files.** `python tools/os_census.py --outside src/game/platform`:
65 files outside `platform` include `windows.h`, so the doc's figure holds.
Of them 17 are fuzz, harness or census files (hosted i686 only). The other 48:

- 24 take nothing from it (empty-stub compile below).
- 16 take `GetEnvironmentVariableA/W` and `DWORD` only: the `BOF3X_` switches.
- 1 takes `_stricmp` only (`dragon_cmd.cpp`).
- 7 take services: `battle_menu_win.cpp` (the FMV player: GDI, a message loop,
  the clock, heap, a thread wait, files), `dat_load.cpp` (file walk, module
  path), `crt_rest.cpp` (environment, `_findfirst32`, `_filelength`,
  `_fileno`, `_stricmp`), `draw_pool.cpp` (`VirtualQuery`/`VirtualAlloc`),
  `faerie_ring.cpp` (`CreateFile`/`ReadFile`), `kind2_object.cpp`
  (`FlushInstructionCache`), `quick_menu.cpp` (temp dir, process id, `VK_F5`).

Check by a second method: `python tools/linux_probe/linux_check.py stub FILE...`
compiles each of the 65 `-fsyntax-only` on `x86_64-linux-gnu` with empty
`windows.h`, `dinput.h`, `io.h`, `malloc.h`. 25 files compile with no error
besides the layout asserts below (24 "nothing" files plus `field_2.cpp`,
whose `io.h` include is dead); the others fail on exactly the names above. The
`#include <windows.h>` count by `grep -rE '#\s*include\s*<windows.h>' src | wc -l`
is 132, against 130 include rows in the census; the two are not accounted for.

A finding for stage LB: every file fails 2,938 `static_assert`s of the generated
`symbols.gen.h` on Linux, because the layout asserts count `long` as 4 bytes
and LP64 makes it 8. (`linux_check.py stub` counts them apart.)

**LA2, low memory.** `lowmem_probe.cpp` (built `-no-pie -Wl,-Ttext-segment=0x10000000`,
run with the image base as argument):

- `mmap_min_addr` 65,536; `randomize_va_space` 2; `overcommit_memory` 0
  (`/proc/sys/vm/*`). This machine only.
- Linked at 0x10000000, the image maps at `0x400000` and at `0xA00000` with
  `MAP_FIXED_NOREPLACE`; write and read of 1,423 pages pass; a `jmp rel32`
  stub placed in the ELF reaches it (+264,241,723 bytes) and returns.
- Linked at the default (`-no-pie`, base `0x400000`): mapping `0x400000` fails
  with EEXIST, as the ELF is there. A small ELF with the image at `0xA00000`
  works; the real engine's size is not known, and must fit in
  `0x400000`..`0xA00000`.
- A PIE ELF (base `0x55...`): the image maps, but the `rel32` stub cannot reach
  the ELF (distance 9.4e13). PIE needs the 64-bit-thunk mode, or a low link.
- Task stacks below 4 GB: fixed at `0x60000` (1 MB, Windows' place), `MAP_32BIT`
  and a hint at `0x20000000` all work, each with a stack switch that saves a
  32-bit word. A 1 GB reserve at `0x40000000` works (it takes `MAP_32BIT`'s
  room, so the order of the reservations is a design item).
- The heap: glibc's default puts 0 of 48 blocks of 64 MB below 4 GB.
  `mallopt(M_MMAP_THRESHOLD)` is capped at 32 MB and does not help.
  `M_MMAP_MAX` 0 forces the `brk` heap and puts 47 or 48 of 48 below 4 GB; a
  block fails when it meets a stack mapping. A hard guarantee needs our own
  arena at the `Crt_malloc` chokepoint.
- The moved base `0xA00000` gives `0xA00000`..`0xF8F000`, which does not meet
  Capcom's range. The ordering tables' 24-bit links keep the image under 16 MB,
  so the base can move inside `0x10000`..`0xA70000`.

Design the probes support: a non-PIE ELF linked at a high address
(`-Ttext-segment`), the data image mapped by `MAP_FIXED_NOREPLACE` at
`0x400000` or the moved base, the stacks and the heap in an arena we reserve
below 4 GB before `main`.

**LA3, the asm.** `python tools/linux_probe/asm_by_abi.py`: 328 `asm`
statements in all (`tools/width_census.py` counts 322 blocks and 6 label
aliases; the 174 earlier in this doc is stale). Of them: fuzz (common) 127,
live common 153, live i686 24, live x86-64 24. The 24 live x86-64 blocks, read
by hand: 10 depend on the Win64 rule and 14 do not (the script's text rule says
11; `Task_Exit` is a false hit, `rcx` is a scratch there).

Depend on it:

- `src/audio/mp3.cpp:205` `Decode` (`rcx`/`rdx`, `subq $32`; the callee
  `DecodeFrame` must be `ms_abi`)
- `src/game/battle/battle_3.cpp:2182` `Battle_StatusTint` (`testb $0x80,%cl`)
- `src/game/field/event_cond.cpp:860` `ScriptFlags_Clear40` (`movl %eax,%ecx`)
- `src/game/field/field_6.cpp:2292` `Scenario_CallA` and `:2333`
  `Scenario_CallB` (`rcx`, and `rdx`/`r8`/`r9` forwarded by tail jumps)
- `src/game/platform/magic_080.cpp:1031` `Cfg_Load` (`leaq 0x20(%rsp),%rcx`,
  frame `$0x68`, callee `Shell_CfgLoadFrame`)
- `src/game/task_switch_x86_64.h`: `Task_RunAll` (:119, pushes the Win64 set,
  a superset, harmless under System V), `Task_SetStackBase` (:167, `rcx` and 40
  bytes of home space for `StackWatch_AtSetStackBase`), `Task_Sleep` (:214,
  argument in `cx`), `Task_Restart` (:233, argument in `rcx`).

The other 14 are `Task_BackToScheduler`, `Task_Exit`, the `jmp` forwarders
(`Sound_StopMusic`, `Sound_ResumeAll`, `Sound_MusicPlaying`, `LoadMenu_Error`,
`Music_Halt`, `Music_IsPlaying`, `SndStream_IsPlaying`,
`ScenaCall_SaveAndLeave`, `Scena15_LeaveAllParty7`), the two in `image_base.h`
and `stackwatch.cpp:303`. A forwarder passes `ecx` on unchanged, so it works
when caller and body share one ABI.

**ms_abi verdict.** One asm text can serve both: `linux_check.py asm` compiles
11 files on `x86_64-linux-gnu` as they are and with
`-D__cdecl=__attribute__((ms_abi))`; the 19 naked functions in them (the six
in `task.cpp` and the rest of the list above that fall in those files) come out
byte-identical. `msabi_probe.cpp` runs the `mp3`, `Battle_StatusTint` and
`Cfg_Load` texts against a callee of each ABI: they are right with an `ms_abi`
callee (`Cfg_Load`'s frame is wrong with a System V one). The `mp3` text is
unchanged; only the compiler's own prologue differs (it saves `rsi`, `rdi`,
`xmm6`..`xmm15`). A sample of 24 files (every 30th non-fuzz `src/game` `.cpp`)
adds no error with `-D__cdecl=ms_abi`. Road: map `__cdecl` to `ms_abi` on the
Linux target. `__stdcall` must stay System V: DXVK-native's headers define it
(and `WINAPI`, `STDMETHODCALLTYPE`) as empty, so the calls into DXVK and SDL
keep their ABI. The second road (a System V variant of the stack-switch
blocks) is not needed.

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

### LB1 as run: the operating-system seam (`src/os`)

`src/os` is a subsystem: one header per service in `bof3::os`, a backend per
system in `src/os/win32` and `src/os/posix` (CMake picks the folder), built as
the static library `bof3x_os`. `os/os.h` only includes the others. No
platform `#if` appears outside the backends; inside them the only one beyond
the folder choice is the `BOF3X_OS_SDL` switch of the POSIX message box. The C++ standard stays C++20, and
where the standard library covers a service the engine uses it; `bof3::os` holds
only what it cannot do.

| Service | Where | Why |
|---|---|---|
| Memory at a fixed address: reserve, commit, protect, release, query, flush | `os/memory.h` | The standard library cannot place pages. |
| Clocks | `os/clock.h` | `game_clock.cpp` (DIV-0022, DIV-0047) keeps `GetTickCount64`'s 15.6 ms step, `GetTickCount`'s wrap and QPC's frequency; `steady_clock` changes all three. |
| Environment, own path, last error, thread and process numbers | `os/env.h` | 113 sites rely on `GetEnvironmentVariableA`'s size contract, which `std::getenv` does not have. The value is a narrow name (below). |
| A narrow name as a path and back, the game's `DAT\AREA*.DAT` walk, case-insensitive name compare | `os/file.h` | The walk is portable code over `std::filesystem`; `Path` and `Narrow`, the narrow encoding, are each backend's. |
| UTF-8 and `wchar_t` | `os/text.h` | Inline over `std::filesystem::path`. Not for a narrow name on Windows. |
| The fatal message box | `os/dialog.h` | `MessageBoxA` on Windows (DIV-0039 is unchanged); stderr and SDL3 on POSIX. |
| SHA-256 | `common/sha256.h` | Our own routine replaces BCrypt; nothing per system. |

Dropped because the standard library covers them: files (`std::ifstream`,
`std::ofstream`, `std::filesystem`), threads, mutexes and events (`std::thread`,
`std::mutex`; no engine call site used them), the heap (`std::malloc`,
`std::calloc`, `std::free`).

**The narrow encoding (review of PR 226, 2026-11-11).** A `char` name in the
engine - one the game builds, one `GetEnv` returns, one handed to the game's
own `Crt_fopen` - is in the system's narrow file-name encoding: the ANSI code
page on Windows (what every one of these sites read through the A calls before
the seam), UTF-8 on POSIX. `os::Path` and `os::Narrow` are the two
conversions, in `src/os/win32/file.cpp` (`MultiByteToWideChar` and back, the
code page the A file calls use) and `src/os/posix/file.cpp` (the bytes as they
are). As first written `Path` read every narrow name as UTF-8 on both systems.
That holds under our own `bof3x.exe`, whose manifest makes the process's
code page UTF-8 (`code page: GetACP() = 65001` in its log), and fails under
the hosted `BOF3.exe`, which gets the machine's: measured on Windows at both
widths with the ANSI code page 1252, `BOF3X_CACHE` under a folder with `é` in
its name left `GetFileAttributesA` finding it and `os::IsDirectory` throwing
`std::runtime_error: locale not supported`, uncaught - hosted, the game ended
before `dat_cache::Configure`'s first line (exit code `0xC0000409`, a log of
14 lines), where the build with the fix logs `DIV-0089    the cache ...
checked: 488 of the manifest's 742 containers held whole` and runs the
self-tests to their end; the same with the game folder itself under such a
name. Now `Path`,
`Narrow`, `IsFile`, `IsDirectory` and `WalkDirectory` do not throw (a name
that does not convert is a name that is not there), the walk matches and
orders names as the system holds them and hands each back in the narrow
encoding, and `ExecutablePath` / `ModulePath` / `ToUtf8` / `ToWide` are marked
as the UTF-8 pair they are, never to meet a narrow name. `tools/os_host`'s
`TestNarrowNames` makes a folder and file under a name outside ASCII by the
system's own wide call and reaches them by `IsDirectory`, `IsFile`, a stream
through `Path`, the walk, a walked name printed into a path for `fopen`, and
`GetEnv`; CI job `os-host-windows` runs the suite against the Win32 backend at
both widths. **For LB8 and the later units:** a name that comes from a wide or
UTF-8 source (the launcher's settings, a module's path) and is to be opened by
narrow code crosses through `Narrow`, not `ToUtf8`; on Windows a path the code
page cannot hold does not open by its narrow name, exactly as before the seam
(the launcher hands the engine its paths through the environment, read by the
A call). Lifting that limit means wide or UTF-8 names end to end, the game's
C runtime included, and is its own unit.

**The walk's cost.** `WalkDirectory` lists and orders the whole folder before
it hands the first entry, because "the first match" must be the same on both
systems. Three callers only ask "is there one" (`dat_cache.cpp`
`LayerDirHasDat`, `dat_load.cpp` at the BGM-layer and installed-layer tests),
all at start-up. What was cheap is done: an entry's kind and size are now
asked only when the entry is handed, so those callers pay the listing and one
entry instead of a `stat` of every match. An "any match" call that stops at the
first name read from the folder would skip the listing's sort too; left for a
unit that finds it on a profile.

**Windows-only, by design:** the launcher, `write_watch`, `gapwatch`, `calltrace`
(harness-only watchers), the PE exe host (`exe_host.cpp`, `exe_icon.cpp`,
`dllmain.cpp`, and the PE-header reader `host.cpp`), and `MiniDumpWriteDump`
in `crash.cpp`. They need the PE format or a Windows API with no portable
form. (`detour.cpp` and `exe_image.cpp` were listed here too until group R23
of the 38-file round: neither needs Windows now; `tools/linux_probe/linux_tree.py`'s
`WINDOWS_ONLY` holds the list.)

**The gate.** `python tools/os_census.py --boundary` (CI job `os-host`) fails
when a live file outside `src/os` and `src/launcher` includes `<windows.h>` or
calls a Win32 name that `bof3::os` replaces. `BOUNDARY_WAITING` in that file
lists the files that still do, each with the unit that removes them (`W`,
`L2`, `L3`). The list may only shrink; an entry that no longer breaks the rule
fails the gate.

**For L2 and L3:** the window (`window.h`), input (`input.h`), fibers
(`context.h`) and the fault catcher (`fault.h`) belong in this same subsystem,
one header each with a backend in `win32/` and `posix/`.

**Not done in L1:** `log.cpp` and `snapshot.cpp` still call the system directly
(the log writer thread, files and fibers); they are listed under L3 because the
log flush runs on the crash path and the snapshot owns the fibers. The LP64
`static_assert` failures LA found are not L1's.

### LB0 as run: the width of `long` (2026-11-17, branch `linux/long-pass`)

Stage LA found that every file fails the generated header's layout asserts on
Linux, because `long` is 8 bytes there (LP64) and 4 on both Windows widths.
This unit removes that. It was run on Windows, at the owner's word of
2026-11-17 to move towards a Linux build; frost's fork held nothing past
`main` `1eda5a70` on that day.

**The measure before.** `tools/linux_probe/linux_tree.py` compiles every
`.cpp` under `src/` but the launcher's and the Win32 backend's
`-fsyntax-only` for `x86_64-linux-gnu`, with no stub for a Windows header and
`__cdecl` as `ms_abi` (LA3's road). On `main` `7999aa1a`, over the 64-bit
build's 813 files: 40 compile, 697 fail on the layout asserts alone, 76 on
something else. (On this machine the compiler is the build's own clang with
`--sysroot=//wsl.localhost/Ubuntu` for the Linux headers; the WSL has g++ and
no clang.)

**The change.**

- `src/game`: `long` and `unsigned long` in code, not in comments or
  literals, are `std::int32_t` and `std::uint32_t`: 5,077 sites in 544 files,
  by a script (a lexer that skips comments, strings, raw strings and character
  literals; `long long` and `long double` untouched). Four headers took
  `#include <cstdint>`.
- `tools/gen_symbols.py` spells every `long` of a `ctype`, `ret` or `params`
  as the fixed-width type in the generated header (`fixed_width`). Until now
  it did so for scalar rows only. `symbols.toml` keeps its spelling: `long`
  there means the image's 4-byte word, and the tools that read its ctypes
  (`band_rows.py`, `magic_rows.py`, `remaining_catalog.py`, `data_widths.py`)
  are unchanged.
- Formats: 84 `%l` conversions whose argument is now `std::int32_t` or
  `std::uint32_t` lost the `l` (25 the 32-bit build's `-Wformat` named, 59
  more the 64-bit build's), and the `fscanf` of `magic_080.cpp:287`
  (`BOF3.CFG`'s four numbers) reads `%d`. Left as they were, each would have
  been wrong on Linux and a warning on Windows.
- `src/hook/draw_order.h`: `LogWalk` takes `const std::uint32_t*`.

**Not changed:** `long` in `src/hook` (256 sites), `src/render` (111),
`src/audio` (85) and the launcher. Most are a cast for a `%lu` or `%ld`,
right on both systems as written; the ones in `src/render` and `src/audio`
are not read yet. Three casts of that kind stay in `src/game`
(`psp_code.cpp:54`, `magic_080.cpp:426` and `:428`). The 41 literals in
`src/game` with an `L` or `UL` suffix are not read either.

**Proof on Windows.** Both presets build with no error and no `-Wformat`
warning. Against a build of `main` `7999aa1a` made in the same folder (the
folder's name is in `__FILE__` strings, so a build elsewhere differs in
every object), object by object, code with relocations and every initialised
data section, a C++ symbol's name read with `long` as `int`:

| Width | Objects | Differ in code | Differ in data |
|---|---:|---:|---:|
| 32-bit | 1,074 | 28 | 29 |
| 64-bit | 818 | 20 | 20 |

Every code difference is an immediate or a displacement: the offset of a
string that follows a format one or more characters shorter. Every data
difference is such a format. All of them are in files whose formats changed.
No instruction differs otherwise. `tools/codegen_same.py` as it is counts a
mangled name that changed (`l` to `i`, `m` to `j`) as a difference, so the
comparison was made with a variant of its file mode that demangles first; it
is not committed.

The census files that key on a line's text are regenerated:
`analysis/width_census.tsv` (the `long` class in `src/game` is the
`long long` rows and the three casts), `analysis/reloc_bare.tsv` (9 keys, the
same lines respelled), `analysis/typed_left.tsv` (7 rows of
`drain_orb_a.cpp`, whose function column read `long`). The other checks of
`checks.yml` that read `src/` pass unchanged.

**Not run** for this part alone: the hosted self-tests (the second part ran
them on both parts together) and the state-hash routes. No ledger entry:
nothing a player sees changes.

**The measure after.** `python tools/linux_probe/linux_tree.py`: 1,055 files
(the fuzz files too), **991 compile for x86-64 Linux, 64 do not**.
`analysis/linux_tree.tsv` lists the 64 with a reason each, and `--check`
fails on a file that is not listed (and on a listed file that now compiles,
so the list only shrinks). It is not a CI job yet: the runner's clang against
this source is not tried.

| Reason | Files | What they are |
|---|---:|---|
| `windows.h` | 28 | the files of `BOUNDARY_WAITING` (L2, L3 and the PE host), the harness watchers of `src/hook`, four harness files of `src/game` |
| `io.h` | 10 | `_findfirst`, `_filelength`: the save and music files, `crt_rest.cpp` |
| `dinput.h` | 9 | DirectInput's key codes (call 4: retired for SDL3) |
| `va_start` | 8 | a variadic function is `__cdecl`, so `ms_abi` here: seven fuzz files and `music_seq.cpp:629` |
| `type` | 5 | `size_t` narrowed in a brace list, overloads that differ by `long` and `int` (`effect_1a.cpp`, `party_action.cpp`, three fuzz files) |
| `name` | 2 | `_strnicmp` (`mp3.cpp`), `_msize` (`sound_ours.cpp`) |
| `d3d11.h` | 2 | `render_d3d11.cpp`, `satpixie.cpp`: DXVK-native's headers (call 1) |

The `io.h` ten, the two names, the five type sites and the eight `va_start`
files were the next unit, below.

### LB0, second part: the 25 files (2026-11-17, the same branch)

The files the list gave under `io.h`, `name`, `type` and `va_start`.

- **Fuzz files are not counted.** The 64-bit host drops every `*_fuzz.cpp`
  (`CMakeLists.txt`), and a Linux build is the 64-bit host, so
  `linux_tree.py` skips them: 802 files, not 1,055. Eleven of the 25 were
  fuzz files.
- **A variadic function has no `__cdecl`.** `__cdecl` is the default on
  x86-32 and means nothing on x86-64 Windows; where it is `ms_abi`, `va_start`
  is an error and the list cannot go to the C library's `v*` calls.
  `tools/gen_symbols.py` writes no convention for a `params` that ends in
  `...` (`Crt_sprintf`, `Crt_sscanf`), and the definitions, the stand-ins and
  the callee tables' pointer types (25 lines) follow.
- **Two pointer types say `__cdecl`** where they take one of ours
  (`effect_1a.cpp:190`, three lambdas of `party_action.cpp`).
- **Six `#include <io.h>` were dead** and are gone.
- **`_stricmp` and `_strnicmp`**, which only Windows' C library has:
  `dragon_cmd.cpp` and `mp3.cpp` use `os::SameName` / `SameNameN` (the same
  ASCII fold), and `Crt_stricmp` is written out as its comment describes the
  original.
- **The save list's search and the file layer's two questions are in the
  seam.** `os/find.h` (`FindFirst`, `FindNext`, `FindClose` into
  `os::FindData`, MSVC 6's `_finddata_t`) and `os/stream.h`
  (`StreamDescriptor`, `DescriptorLength`): the C runtime's calls on Windows,
  the portable walk and `fstat` on POSIX. POSIX has no creation time, so
  `time_create` and `time_access` read -1, as MSVC says them for a file system
  without them.

**A weakness of the 64-bit Windows build, found by the new test and
removed.** The runtime's search handle is an `intptr_t` and the game keeps a
32-bit word. In `tools/os_host`'s 64-bit process the handle lay above 4 GB
and `_findnext32` faulted on the narrowed one. The engine's own process has a
low heap and never showed it. On x86-64 the Win32 backend now gives the game
the number of a slot that holds the handle; on x86-32 the word is the handle,
as before.

**Proof.**

| Check | Result |
|---|---|
| Both presets | build, no error, no `-Wformat` warning |
| Objects against the first part's commit, both widths | 4 differ in code beyond a constant: `mp3.cpp`, `dragon_cmd.cpp`, `save_menu.cpp`, `crt_rest.cpp` - the four with a changed function. On the 64-bit build five more differ in a `__LINE__` constant (a removed include line) |
| `os_tests` | Win32 backend, 32-bit: 112 checks, 0 failed; 64-bit: 113, 0 failed; POSIX backend (g++ 15.2 in the WSL): 113, 0 failed. `TestFindAndStreams` is new |
| The hosted shadow suite (`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*'`, 32-bit, a clean launcher folder, the PC install) | runs to `self-test only: done`; 922 summary lines, each `0 MISMATCHES`; `crt_rest`'s among them: `_stricmp` 60,000 rounds, `_findfirst / _findnext` 2,029 matches over 5 patterns, the file layer |

**Not run:** the state-hash routes; the 64-bit game itself with the handle
slots (its save list was not opened).

**The measure now.** 802 files, **764 compile for x86-64 Linux, 38 do not**:

| Reason | Files | What they are |
|---|---:|---|
| `windows.h` | 26 | `BOUNDARY_WAITING`'s files (L2, L3, the PE host), the harness watchers of `src/hook`, two harness files of `src/game` |
| `dinput.h` | 9 | DirectInput's key codes (call 4: retired for SDL3) |
| `d3d11.h` | 2 | `render_d3d11.cpp`, `satpixie.cpp`: DXVK-native's headers (call 1) |
| `name` | 1 | `sound_ours.cpp`: `_msize`, the exact size of a heap block. glibc's `malloc_usable_size` answers a larger number, so this waits for the arena at the `Crt_malloc` chokepoint that LA2 found the heap needs anyway |

Every file left belongs to a unit section 6 already names: L2 (the window,
the keys), L3 (the fibers, the crash catcher), the renderer's headers, the
heap. A compile is not a link: stage LC's items (the ELF preset, the
low-memory image, DXVK-native) are untouched.

### The 38 files, staged as one round (2026-11-17; not launched)

**Staged, not started.** It launches at the owner's word, after this branch
is on `main`, and after the split with frost is settled (item 1 of "For the
owner" below). It is the rest of stage LB cut by file, so that groups can run
side by side: no file is in two groups. The site counts are the rows of
`analysis/os_census.tsv` for the file, includes left out
(`python tools/os_census.py`, this branch).

**Goal:** `python tools/linux_probe/linux_tree.py` answers 0 files that do
not compile, or names each file that stays a Windows file by design and is
left out of the Linux source list. Still no link: stage LC is after it.

| Group | Files | Sites | What it does | Unit |
|---|---|---:|---|---|
| R1, the harness that proves a Linux run | `statehash.cpp`, `input_script.cpp`, `wav_dump.cpp`, `stallwatch.cpp`, `boss_harness.cpp`, `scenario_harness.cpp` | 8, 39, 53, 81, 8, 2 | Onto the seam. The state hash and the input queue are stage LC's gate, so they must run on Linux; `statehash.cpp` uses two type names alone. `input_script.cpp` posts window messages and names `VK_F1`..`VK_F12`: it takes R4's key names | LB7 |
| R2, the watchers that stay Windows files | `gapwatch.cpp`, `write_watch.cpp`, `calltrace.cpp` | 153, 43, 100 | They work by debug registers, guard pages and a vectored handler. Proposed: Windows-only, out of the Linux source list, each switch a loud refusal there (rule 4: no silent stub). A port is a later, separate piece | LB7 |
| R3, the PE host | `exe_host.cpp`, `exe_icon.cpp`, `dllmain.cpp`, `host.cpp`, `detour.cpp`; `exe_image.cpp` | 118, 154, 15, 2, 4; 21 | The first five are Windows by design and leave the Linux source list; what a Linux start-up needs from `host.cpp` and `detour.cpp` (the image's base, 2 and 4 sites) is written down for LC. `exe_image.cpp` builds the data image from a disc-built install and LC needs it: its 21 sites (the narrow and wide text calls, `_wfopen`) go to the seam | LC's start |
| R4, the keys | `bindings.cpp`, `quick_menu.cpp`, `msgbox.cpp`; `menu_list.cpp`, `cfg.cpp`, `rest_2h.cpp`, `shop_win_2.cpp` | 24, 2, 3; 0, 0, 0, 0 | One header of our own for the key numbers the game stores (`DIK_*`, 24 uses in `bindings.cpp`; `VK_F1`, `VK_F5`): the numbers are the save's and the configuration's, so they do not change. The last four include `dinput.h` and the census finds no name from it: the include goes, the build says if it was dead | LB3 |
| R5, the keyboard and the pad on SDL3 | `d_input.cpp`, `pad_read.cpp`, `rdata_consts.cpp` | 36, 2, 5 | Call 4: DirectInput retired on both systems. The device, its data format and the check of `c_dfDIKeyboard` against Capcom's go; SDL3's keyboard and gamepad fill the same words. A player can tell (which pads are seen, the buttons' numbers): a ledger entry and a `BOF3X_OFF` switch while both paths exist | LB2, LB3 |
| R6, the window | `win_main.cpp`, `display_setup.cpp`, `d3d.cpp`, `magic_080.cpp`, `battle_menu_win.cpp` | 23, 9, 15, 131, 69 | The window, its message loop, the monitor and the modes on SDL3. `battle_menu_win.cpp` has a window procedure and GDI calls of its own (34 window sites): read first, it may be the movie's picture (LB4). `win_main.cpp`'s 7 fault sites are R8's | LB2, LB4 |
| R7, the fibers | `snapshot.cpp`, the fiber of `render_d3d11.cpp` (lines 1049..1065) | 84 (7 fiber, 52 file), 5 | The render call's own stack and the snapshot's by the task switch that is already ours, or a thread. `snapshot.cpp`'s 52 file sites go to the seam in the same group | LB6 |
| R8, the crash catcher, the log, the sound's events | `crash.cpp`, `stackwatch.cpp`, `cpu_context.h`, `log.cpp`, `sound_sdl.cpp` | 104, 84, -, 72, 16 | The report (`CRASH n:` lines) portable, the catcher per system (SEH and the minidump on Windows, signals on Linux). The log's writer thread and `sound_sdl.cpp`'s events on the C++ library's mutex and condition variable | LB7 |
| R9, the renderer against DXVK-native's headers | `render_d3d11.cpp`, `satpixie.cpp` | 209 (169 graphics), 51 (48 graphics) | The headers fetched pinned for the Linux target alone; the 13 process and window sites of `render_d3d11.cpp` to the seam. Windows keeps Microsoft's headers (call 1). R7 owns the fiber lines of the same file, so R9 merges after R7 | call 1 |
| R10, the heap | `sound_ours.cpp` | 3 | The arena at the `Crt_malloc` chokepoint that LA2 asks for, with a size query of its own in place of `_msize`. On Windows the arena is behind a switch until the routes pass with it | LC's start |

**Two waves, since one round runs at a time and the owner's eye is the
bottleneck.**

- **Wave 1, nothing a player can tell:** R1, R2, R3, R4, R8, R10. Proof is
  the gate line below with no live check by the owner.
- **Wave 2, what a player can tell:** R5, R6, R7, then R9. Each has a
  `BOF3X_OFF` switch, a ledger entry where it is one, and ends in one batched
  check by the owner: the window modes, a pad, the keyboard's bindings, the
  three movies.

**The gate line, each group, before its hand-back:**

- both presets build with no error;
- `python tools/os_census.py --check --boundary` with the group's rows gone
  from `BOUNDARY_WAITING` (the list may only shrink);
- `python tools/linux_probe/linux_tree.py --check` with the group's rows
  gone from `analysis/linux_tree.tsv`;
- `os_tests` on the Win32 backend at both widths and on the POSIX backend,
  with a test for every call the group adds to the seam;
- the hosted shadow suite (`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*'`, a clean
  launcher folder) and the exe-host and moved-base runs;
- `data_census --strict`, `reloc_census --check`, `ledger_check.py`;
- R5 to R8 and R10, which change what runs: the ten state-hash routes at
  the coordinator's merge, one at a time on the machine.

**What two groups share, and how it merges.** `src/os/os.h` (each group adds
an include line: keep both), `tools/os_census.py`'s `BOUNDARY_WAITING` (each
removes its own rows), `CMakeLists.txt`'s source lists. The three censuses
(`os_census.tsv`, `linux_tree.tsv`, `width_census.tsv`) are regenerated by
the coordinator after each merge, never merged by hand. Agents number
nothing in the ledger or the defect list.

**For the owner, before a launch:**

1. **Who runs which group.** HANDOFF gives the round's next units to frost
   on his fork, and his `linux/native` held nothing past `main` when this was
   written. Proposed: wave 1 here (it is mechanical and needs no Linux
   machine), and R5, R6, R7 and R9 offered to frost first, since they are his
   units L2 and L3 and his machine has the GPU that proves R9.
   **Decided (the owner, 2026-11-18): wave 2 also runs here**, in worktrees
   and harness roots apart from the play install; frost takes the round
   over when he is ready. R9's proof on a Linux GPU stays his.
2. **R2:** the three watchers Windows-only, or ported.
3. **R5's switch:** how long DirectInput stays beside SDL3 (proposed: one
   release). **Decided (the owner, 2026-11-18): one release, and the player
   chooses** between the two paths in it, by a launcher option; SDL3 is the
   default, so that the release tests the path that stays, and DirectInput
   is the fallback. **Changed the same day, after R5's read pass: no
   release with both.** The pad has been on SDL3 since DIV-0050, so the
   choice was the keyboard's alone; the keyboard goes to SDL3 with no
   launcher option and no DirectInput path kept for a player. A ledger
   entry for what a player can tell (the keys whose SDL scancode differs
   from DirectInput's) is still owed at the merge.
4. **R10:** the arena on Windows too, or on Linux alone (proposed: both, so
   that one heap is tested; behind a switch first).

**Not measured for this staging:** that the four `dinput.h` includes of R4
are dead (the census finds no name; no build without them was made); what
`battle_menu_win.cpp`'s window is for; whether the task switch can carry
Direct3D's calls in R7 (the fiber exists because the render call wants a
stack of its own); the size of R5 on the PSX-button and binding pages of the
launcher, which write the numbers R4 keeps.

### R10 as run: the arena (2026-11-17, branch `linux/w1-r10`)

**Measured first.**

- **The chokepoint** (`grep -rnE "(std::)?(malloc|calloc|free)\s*\(|\.(malloc|free)\s*\(" src/game src/render src/audio src/common`):
  three entries, `Crt_malloc`, `Crt_calloc`, `Crt_free` (`crt_rest.cpp`), bound at Capcom's
  `0x5B9660`, `0x5BA221`, `0x5B9577` as a set (crt-rest.md 2.6). There is no `Crt_realloc`: Capcom's
  `realloc` `0x5C2F8E` is not ours and nothing reaches it (crt-rest.md 2.6). Hosted, the callers are
  ours, the decoder's (under `BOF3X_MP3=original`) and the executable's runtime start-up.
- **Others that allocate memory a 32-bit word holds**, read but not changed here: the SOUND self-test's
  stand-ins (`sound_ours.cpp`, `std::malloc`, `Address(stream)`: moved to `Crt_malloc` below);
  `music_seq.cpp:634` `TestMalloc` (`std::malloc`, into `Music_File`); `render_shim.cpp:53` `Alloc`
  (`std::calloc`, the surface objects in `Slot32<ComObject>`); `mp3.cpp:109` `new Decoder` (its frame's
  address in a game word, a Fatal above 4 GB today). On Linux each needs the arena too, or an
  `operator new` / `calloc` of its own below 4 GB: stage LC's.
- **The three `_msize` sites**, all `sound_ours.cpp`: `HeapBlockBytes` (`:881`), the quicksave's SOUND
  record writing a bank's sample data, the last stream's file and a non-track music's data by their
  exact length (`:951`, `:990`, `:1018`); and two checks of the SOUND self-test (`:1672`, `:1703`) that
  the restored blocks have the take's length.
- **How the 64-bit Windows build keeps its heap low today:** by placement, not code
  (x64-build-design.md 2 and 5: a fixed-base executable's bottom-up heap, 44 of 48 blocks of 64 MB
  below 4 GB), guarded by `LowBlock` at the chokepoint. The design named this arena as the fallback.
- **Threads:** the executable imports no `CreateThread` (crt-rest.md 2.1); our DLL's threads (log
  writer, crash reporter, stall watch, SDL's audio callback) use `std::malloc`, not the chokepoint.
  So one thread calls it, by this reading; the arena locks anyway.
- **Size:** the largest file the game reads whole is `SND\KARA.DAT`, 4,310,831 bytes
  (`find bof3/ -type f -size +4M`). The game's peak heap was not measured.

**The design.** `src/os/heap.h`, three parts:

- `HeapExactSize` / `HeapHasExactSize` per backend: `_msize` on Windows (`win32/heap.cpp`), no answer
  on POSIX (`posix/heap.cpp`).
- `MemReserveBelow4G(size, floor)`, portable over `MemQuery` and `MemReserve`: the free runs in
  `[floor, 4 GB)`, the highest that holds the size, 64 KB aligned.
- `LowArena` (`src/os/heap_arena.cpp`, portable): one reservation, committed in 64 KB steps as the
  used end rises; blocks with a 16-byte header {size, size below, requested, tag}, 16-byte aligned;
  first fit from a free list; a freed block merges with free neighbours both ways, and one that
  reaches the used end gives the room back. `Alloc(0)` is a block of its own of size 0; above
  `0xFFFFFFE0` (MSVC 6's `_HEAP_MAXREQ`) null; `Calloc` refuses a product past it; `Realloc` is C's
  (null is `Alloc`, 0 frees, in place when it can, moved when not, null and the block kept when there is
  no room); `SizeOf` answers the request exactly; one `std::mutex`. `Open` takes `[2 GB, 4 GB)` first
  (above every bind-table top byte, `bind.cpp`, and above `MAP_32BIT`'s room), then the system's own
  place if it lies below 4 GB (a 32-bit process), then `[64 KB, 2 GB)`.

The chokepoint (`crt_rest.cpp`): with the arena on, `Crt_malloc` / `Crt_calloc` take its blocks
(256 MB reserved; a request it cannot hold is a Fatal naming the 256 MB, the live bytes and the used
end); `Crt_free` frees its blocks and hands any other pointer to the toolchain's `free`, as before
(a block of the runtime's own heap, or a `std::malloc` one); `crt_rest::BlockBytes` is the size
query, the arena's record for its blocks, else the C runtime's `_msize`, else a Fatal.
`sound_ours.cpp` asks `BlockBytes` in place of `_msize`, and its self-test takes its blocks from
`Crt_malloc` / `Crt_free`, so the size query answers for them and they lie below 4 GB.

**The switch: `BOF3X_ARENA`**, read once at the first heap call through `os::GetEnv`: `1` on, `0`
off, anything else a Fatal. Unset, it is on where the C runtime has no exact size
(`HeapHasExactSize()` false: Linux), off on Windows; `0` where there is no exact size is a Fatal. On, the
log has one line: `heap        the arena (BOF3X_ARENA): 256 MB reserved at ...`. Off, the three entries
call the toolchain's `malloc`, `calloc` and `free` as before, after one test of a static; the block
size is `_msize` through the seam. `tools/ledger_switches.py` lists the name (`env`). No ledger entry:
where a block lies and how the heap is cut is nothing a player sees.

**Proof.**

| Check | Result |
|---|---|
| `os_tests`, `TestHeapArena` | Win32 32-bit: 153 checks, 0 failed (the arena at `0x015F0000`: no free run above 2 GB in a 32-bit process, so the system's place); 64-bit: 157, 0 failed (at `0xFF000000`); POSIX (g++ 15.2, WSL): 157, 0 failed (at `0xFF000000`) |
| What the test does | the exact-size call per system; a reservation below 4 GB above a 2 GB floor; zero-size, too-large and double-free cases; 20,000 random rounds of malloc, calloc, realloc and free against a model (sizes, 16-byte alignment, every block below 4 GB, contents kept, calloc zeroed, invariants every 500 rounds); free all, then the used end at 0; 4,177 blocks of 4,000 bytes to the end, every other one freed, the rest freed, one block of 16 MB less 4 KB; a block grown into the reserved room and shrunk in place; four threads at once |
| The test fails when it should | two mutations, built in the WSL: no merge with the block below (at least 6 checks fail, the output cut at 6: the invariants and the used end after freeing all), `SizeOf` answering the usable size (4 fail) |
| The hosted shadow suite, 32-bit (`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*'`, a clean launcher folder, the PC install) | arena off: `self-test only: done`, 922 `MISMATCHES` lines, each 0; arena on (`BOF3X_ARENA=1`): the same, 922 and 0, the arena at `0x18440000..0x28440000` (no free run above 2 GB, so the system's place), the SOUND self-test and `crt_rest`'s heap fuzz (200 rounds of 64 blocks) among them |
| The 64-bit exe host's start-up self-tests (`BOF3X_SELFTEST_ONLY=1 ... --host exe`, `build-x86_64`) | off and on: exit 0, `self-test only: done`; on, the arena at `0xF0000000..0x100000000`; the two logs differ only in process and thread numbers and one free-run size below 16 MB |
| `linux_tree.py` | 804 files (the two new), 767 compile for x86-64 Linux, 37 do not; `sound_ours.cpp` is not among them |

**Not verified:** the game's peak heap with the arena (the used end's high-water mark is kept,
`LowArena::HighWater`, and not logged); the ten state-hash routes with it on or off (the
coordinator's, at the merge); a played game with it on; anything on Linux beyond the compile and
`os_tests` (no link exists).

#### R3: what a Linux start-up needs from the PE host

Group R23 (wave 1), 2026-11-17. `host.cpp`, `exe_host.cpp`, `exe_icon.cpp`
and `dllmain.cpp` are in `tools/linux_probe/linux_tree.py`'s `WINDOWS_ONLY`:
each one reads or patches a PE image in the process or is the DLL's entry.
`detour.cpp` is not: its only PE part was `VerifyImage`, the hosted build's
check of `BOF3.exe`'s header, which moved to `host.cpp` (it is called from
`dllmain.cpp` alone). The rest of `detour.cpp` (`Inject`, `RetargetCall`,
`PatchBytes`, `CloneOriginal`, the name lists) compiles for Linux and
already takes its exe-host branch by `IsExeHost()`.

`exe_image.cpp` (the data image from a disc-built install) is on the seam:
`BOF3X_EXEIMAGE` is read by `os::GetEnvW` and `os::ToUtf8`, and its files
are opened by `std::ifstream` on `os::PathOfUtf8` (new in `os/file.h`: a
UTF-8 name as a path, a backslash read as a separator, ill-formed UTF-8
refused; Windows converts with `MB_ERR_INVALID_CHARS` as the file did, POSIX
checks the bytes; `TestUtf8Paths` in `tools/os_host`). The three watchers of
R2 stay Windows files; on another system `src/hook/windows_only.cpp` defines
their entry functions, each switch a `Fatal` when it is set.

The functions the rest of the engine calls in the four files, measured with
`grep -rnE '\b(<name>)\(' src` outside the four files and their headers
(comment lines left out), and what a Linux start-up must supply:

| Function | File | Called from | A Linux start-up must supply |
|---|---|---|---|
| `DllMain` | `dllmain.cpp` | the Windows loader | Nothing: it is the hosted build's start-up, and a Linux build is the exe host only (`IsExeHost()` true). Its order (log, host, image check, `InjectAll`, the watchers) is the template for `main`'s |
| `Engine_Main(hinstance, command_line, show)` | `exe_host.cpp` | `bof3x.exe`'s entry (`tools/make_exe.py`, the import `bof3x.dll!Engine_Main`) | A `main` that does the same steps in the same order: refuse the hosted switches; reserve the linked span 0x400000.. (`Fence`); check the layout; make `.stub` no-access (`TrapStub`); copy `.data` from `BOF3.exe` or from `ExeImage_Source` (`CopyData`, the disc-built image - `exe_image.cpp`, which now compiles for Linux); move the pointer words (`Rebase`); fill the import slots with our functions (`BindSlots`); `SoundSdl_Start`; the x87 word (`SetX87`); `Crash_Start`; `InjectAll`; the watchers' `_Start` calls; `Game_WinMain`. On Linux the image is not mapped by a loader: the start-up maps `bof3x.exe`'s sections (or a layout of its own) at a fixed low address itself |
| `ExeFile_Present()`, `ExeFile_Read(va, to, n)` | `exe_host.cpp` | `dat_load.cpp:848`, `magic_080.cpp:803`, `rdata_consts.cpp:37`, `:109`, `shell.cpp:113`, `:115`, `exe_image.cpp:617`, `:619`, `:1308`, `:1314`, `exe_own.cpp:350`, `:356` (12 lines in 6 files) | A reader of the player's `BOF3.exe` file: whether it is there, and `n` bytes at a VA of its `.text`, `.rdata` or `.data` from the file's section table. This is a file read and the PE file format, not a Windows call: it can be portable code over `std::ifstream` |
| `ExeIcon_Frame(hwnd)` | `exe_icon.cpp` | `magic_080.cpp:1017` | The window icon (DIV-0095) from the same picture through SDL3's `SDL_SetWindowIcon` once the window is SDL3's (R6). `exe_icon.cpp`'s picture code (the VRAM read, the Lanczos scale) is portable; the icon, WIC and COM parts are not |
| `ExeIcon_SelfTest()` | `exe_icon.cpp` | `exe_host.cpp:651` only | The same self-test against the reference `.ico`, without WIC's decoder |
| `DetectHost()` | `host.cpp` | `dllmain.cpp:45` only | The host's identity and base without the main module's PE header: on Linux the host is the exe host, and the start-up knows where it mapped the image. It must still make the 16 MB window check and the `BOF3X_IMAGEBASE` check and set `Bof3_ImageMove` |
| `VerifyImage()` | `host.cpp` (from `detour.cpp`, this group) | `dllmain.cpp:47` only | Nothing: hosted only |
| `Host()`, `IsExeHost()` (inline in `host.h`) | `host.cpp` | 33 lines in 18 files (`grep -rlE '\b(IsExeHost\|Host)\(\)' src`) | `Host()` answering `HostKind::Exe` once the start-up has set it |
| `CodeLo()`, `CodeHi()`, `InCode()` (inline) | `host.cpp` | `calltrace.cpp:310`, `:311`, `crash.cpp:328`, `:329`; `InCode` in `labels.cpp:454`, `code_bytes.cpp:58`, `detour.cpp` (4 lines) | `.stub`'s range (`exe_layout::kStubVa`, `kStubSize`): no PE read needed |
| `Bof3_ImageMove` (a variable) | `host.cpp` | the asm of `field_6.cpp` (4 sites) and `task_switch_i686.h` / `task_switch_x86_64.h` (14) | The same 32-bit word: image base minus 0x400000 |

**Proposed for LC:** split `host.cpp` into its portable part (the state,
`Host`, `CodeLo`, `CodeHi`, `Bof3_ImageMove`, the window and
`BOF3X_IMAGEBASE` checks) and a per-host part that answers the base and the
identity (`DetectHost` reading the PE header on Windows, the Linux start-up
on Linux). Not done here: it changes the Windows file for no Windows gain.

**Found on the way, for LC:** `ImageBase()` (`src/common/image_base.h`, not
this group's) reads the base from the Windows process environment block
(`fs:0x30` / `gs:0x60`). It compiles for Linux, so `linux_tree.py` does not
show it, but on Linux that read is not the image's base; LC must give it
another source (a variable the start-up sets, read once) before anything
places an address.

### Wave 1 as run (2026-11-17, branch `linux/wave1`, not merged)

The owner's word on 2026-11-17: the round runs here. Wave 1's six groups ran
as five agents (R2 and R3 as one), each in a worktree from `b0955695`, and
were merged one at a time into `linux/wave1`. Calls 2 and 4 stand as
proposed: the three watchers are Windows files, the arena is on Windows
behind a switch.

| Group | What it left | Its own proof |
|---|---|---|
| R4 | `src/input/key_codes.h` (`kDik*`, `kVk*`, the SDK's values, asserted against the SDK in `src/launcher/key_codes_check.cpp`); the four `dinput.h` includes were dead | Objects equal but for `__LINE__` and one format string; the hosted suite not run, on that ground |
| R10 | The arena, "R10 as run" above; `BOF3X_ARENA` | Hosted suite off and on, 922 lines of 0 each; the 64-bit exe host's start-up tests off and on |
| R2, R3 | `WINDOWS_ONLY` in `linux_tree.py`; `src/hook/windows_only.cpp` (a Fatal for `BOF3X_CALLTRACE`, `BOF3X_GAPWATCH`, `BOF3X_WRITEWATCH` off Windows); `detour.cpp` and `exe_image.cpp` on the seam (`os::PathOfUtf8`); "R3" above | Hosted suite 922 of 0; the exe-host self-test. `exe_image.cpp`'s reading of a disc-built image not exercised |
| R1 | `os/module.h`, `stack.h`, `sampler.h`, `local_time.h`, `named_event.h`, `path_file.h`; five harness files compile; `BOF3X_STALLWATCH` is a Fatal off Windows (no sampler there yet) | Hosted suite 922 of 0. No route, no WAV dump in a game |
| R8 | `os/event.h`, `thread.h`, `out_file.h`; `crash.cpp` the portable report, `crash_win32.cpp` the old catcher, `crash_posix.cpp` a signal catcher; `stackwatch_win32.cpp` and a refusing `stackwatch_posix.cpp`; `log.cpp` and `sound_sdl.cpp` off Windows' API; `tools/crash_report.py --log` | Hosted suite 922 of 0; a crash on purpose before and after, the same seven `CRASH` lines (32-bit, exe host); `tools/linux_probe/crash_catch_test.sh` in the WSL, nine modes. `music_heard.py`: two ring lines only, the run was stopped |

**At the merge.** Both groups R1 and R8 added `src/os/{win32,posix}/thread.cpp`:
R1's keeps the name, R8's is `thread_start.cpp`. The other conflicts were
both-sides-appended (`os.h`, `os_tests.cpp`, `linux_tree.py`,
`os_census.py`, this file).

| Check on `linux/wave1` after the five merges | Result |
|---|---|
| Both presets | build, no error |
| `os_tests` | Win32 32-bit 234 checks, 64-bit 237; POSIX (g++ 15.2, WSL) 232; 0 failed |
| `os_census --check --boundary` | ok, 11 files wait |
| `linux_tree.py` | 806 files, **794 compile for x86-64 Linux, 12 do not**, 9 Windows files by design |
| `width_census`, `reloc_bare`, `data_census --strict`, `ledger_check` | ok after the censuses were regenerated |
| `reloc_census` | `range_cmp` 83 to 85, on purpose: `log.cpp`'s queue size is 4 MB, which is `0x400000`; a size, not an address |

The 12: `win_main.cpp`, `display_setup.cpp`, `d3d.cpp`, `magic_080.cpp`,
`battle_menu_win.cpp` (R6); `d_input.cpp`, `pad_read.cpp`, `rdata_consts.cpp`
(R5); `snapshot.cpp` (R7); `render_d3d11.cpp`, `satpixie.cpp` (R9);
`input_script.cpp`, which waits for R6's window (lines 960..969) and takes
R4's key names (lines 221..223, 1302..1303).

**The ten state-hash routes, run on the merged branch 2026-11-18** (`d4f90201`,
the exe host, an ini-less launcher copy, the conditions of
[`state-hash.md`](state-hash.md) section 6; a copy of the recording
session's `chain3.py`, no compiler seen during any run; ours as
`cn_<route>_w1`):

- `check` against the `orig5` / `orig5b` pair: on all ten routes the pages
  and their first tick, last tick and tick count are those of `m1116`'s own
  check (`D:\bof3x-runs\orig1116\check_<route>.txt`), compared line for line.
- `diff` against `cn_<route>_m1116` (ours at `main` `df083903`, before the
  Linux work and before PRs #256 to #258): **identical on every compared
  tick on seven routes** (field_menu 1,409; menu_screens 1,729; combat
  2,561; shop 3,137; dragonTransform 4,289; cutsceneAndNue 7,425; whelpBoss
  13,185). On worldMapAndAreaTransition, caughFish and masterAndManillo one
  page differs, `0x6C3000`, from the tick the route's glyph pages begin
  (1,098; 395; 324) to the end. **It is run noise:** a second run of the
  same launcher copy (`cn_<route>_w1b`) differs from the first on that page
  over the same ticks, and from `m1116` likewise. Which bytes of the page
  were not read (no dump was taken).

**The self-tests on the merged branch, 2026-11-18** (the same build, clean
folders under `C:\Users\kerok\bof3x-runs\lbw1\coord`, `--no-config`):
hosted with `BOF3X_SHADOW='*'` - the log ends `self-test only: done`, 922
`MISMATCHES` lines, every one 0; the exe host - game process exit code 0,
10,388 log lines, 0 FATAL; the exe host at `BOF3X_IMAGEBASE=0xA00000` - the
same three figures, the span `0x400000..0x98F000` reserved no-access.

**Not run on the merged branch:** `music_heard.py` to its end, a game with
`BOF3X_ARENA=1`.

**A player on Windows:** nothing by intent, so no ledger entry. Under test
switches: the stall log's module names are UTF-8; a failed seek of the WAV
dump is a Fatal (it was ignored); two state-hash messages are one.

**For stage LC, found by the groups:** `ImageBase()` reads the Windows
process block (above); the game's `CreateEventA`, `CloseHandle` and
`MsgWaitForMultipleObjects` import slots need stand-ins on `os::Event`;
memory the game holds in 32-bit words outside the arena
(`music_seq.cpp:634`, `render_shim.cpp:53`, `mp3.cpp:109`, SDL's sound
objects not traced); `os::CurrentStack()` on POSIX does not know a stack
the program switched onto; the crash catcher's alternate stack covers the
arming thread alone; `tools/input_run.py`'s frozen shots have no Linux side.

**A fault of the run.** One agent started the launcher on its worktree's
empty `bof3` folder and a dialog stood on the owner's screen. The brief
named the right form for the self-test and did not forbid other forms: the
next brief says that a worktree holds no game data, and that a run not in
the brief is not made.

### Wave 2's read passes (2026-11-18; read, nothing built or run)

Three agents read the source for the three questions the staging left
unmeasured. Each finding below is from reading at `main` `5683387a`; the
lines are theirs and none was checked by a build or a run.

**R6: `battle_menu_win.cpp`'s window is the movie player, and it is live.**

- `tools/move_funcs.py` built the file from eight, `fmv_play.cpp` among
  them (its line 1). All its window and GDI sites are `Fmv_Play` (:1017)
  and `Fmv_WndProc` (:1185): lines 127..302 and 1017..1221. The picture is
  `StretchDIBits` (:220..238); the player subclasses the game window's
  procedure (:1066) and runs a message pump of its own (:1107..1155).
- Its one caller is `Game_WinMain`, `magic_080.cpp:850..851`: after the
  window is made, before `Display_Setup` (:858). The movies play before a
  Direct3D device exists.
- `magic_080.cpp` is three jobs: the window, its geometry and its procedure
  (:291..348, :562, :841..847, :909..916); the desktop's size (:830..834);
  two `_wfopen` calls for the placement file (:284, :298).
- `src/os` has no window, event or present service (`os.h:4`), and no
  `SDL_CreateWindow` is anywhere in `src`.
- Not reconciled: the agent's count of `magic_080.cpp`'s rows was about 136;
  the staging says 131.

**R5: the pad is on SDL3 already; DirectInput holds the keyboard alone.**

- `DInput_Init` (`d_input.cpp:164..200`) opens `GUID_SysKeyboard` only.
  `DInput_EnumJoystick` (:210..231) is reached through
  `BOF3X_ORIGINAL=DInput_Init` alone. The pad is `src/input/pad_sdl.cpp`
  (DIV-0050, 2026-09-24).
- `Pad_Read` fills `Key_State[256]`, indexed by DIK number, and reads the
  32 pairs of `Key_Table`. `Key_State` is game memory that the state hash
  reads: an SDL reader must fill it byte for byte the same.
- The stored numbers: `key.NAME=` lines of `bof3x.ini` and lines 3 on of
  `BOF3.CFG` hold DIK numbers; `pad.INPUT=` lines hold SDL's names. None is
  in a save. The launcher takes a key by a `WH_KEYBOARD` hook
  (`config_dialog.cpp:1242..1263`) and never opens DirectInput: its pages
  need no change.
- No table from `SDL_Scancode` to DIK numbers is in the tree.
- The check of `c_dfDIKeyboard` (`rdata_consts.cpp:62..139`) also writes
  the format's header into the image, so that the original `DInput_Init`
  runs.
- Not verified: how SDL3 on Windows reads the keyboard, and whether the
  window messages `input_script.cpp` posts reach it; that SDL's keyboard
  state needs an SDL window (so R5 is after R6).

**R7: neither the task switch nor a thread; a call in the seam that runs a
function on a stack of its own.**

- The render fiber (`render_d3d11.cpp:1013..1066`, commit `340d3739`) is
  there because the game presents from inside a task on a 16 KB slice of
  the main thread's stack, and the first DXGI present ran off it
  ([`render-backend.md`](render-backend.md) 4). The snapshot's
  (`snapshot.cpp:636..680`, commit `e04db5b4`) is there so that a take
  copies slice 0 as the tasks left it. Both ask 256 KB committed of 1 MB.
- The task switch (`src/game/task_switch*.h`) is the game's scheduler: four
  fixed records, slices of 0x4000 or 0x8000 bytes, no System V half. Its
  slices are the stacks the fibers were made to leave.
- A thread: DXGI sends messages to the window's thread, which would wait on
  the hand-off; the hosted self-test runs under the loader lock, where a
  thread cannot start (`snapshot.cpp:296..299`).
- Proposed: `os::RunOnOwnStack`, call and return. Windows keeps its fibers
  behind it (they keep the thread block's stack bounds right, so nothing
  changes there and no ledger entry); POSIX gets a mapped stack with a
  guard page and a switch of its own, and registers the range so that
  `os::CurrentStack()` answers for it (wave 1's gap).
- `snapshot.cpp`'s 52 file sites: the seam covers the writes, reads and
  name calls. It lacks a rename that refuses an existing target (:562), a
  way for the self-test to make the rotation fail (:1068..1070 hold a file
  open with no sharing), and neutral error codes (:615, :1088).
- Not verified: the stack depth DXVK and a Vulkan driver need; whether a
  snapshot participant calls the renderer (nesting).

**What the passes change in the staging.**

- R5 is the keyboard alone, engine alone, and runs after R6 (the owner's
  call 3 above, as changed).
- R6 splits: **R6** the window, its procedure and its loop on SDL3
  (`magic_080.cpp`, `win_main.cpp`, `display_setup.cpp`, `d3d.cpp`), and
  **R6m** the movie's picture and its events, after R6. Before either, the
  movie's code moves back to a file of its own, so that the battle menu's
  code is in no group.
- R7 is the seam call and `snapshot.cpp`'s file sites. It shares no file
  with R6 and can run beside it.
- The order: R6 and R7 side by side; then R5, R6m and R9.

**For the owner:** where the movie's frame is drawn (R6m) - an SDL texture
on the window before the device exists, or the renderer brought up before
the movies. Proposed: the renderer, since tier 2 has no second way to put
a picture on a window that DXVK also owns; R6m's agent measures whether
the device can be made before `Display_Setup` first.

### Wave 2 as run (2026-11-18, in progress; branch `linux/wave2`, not merged)

Launched at the owner's word. Merged into `linux/wave2` so far, one at a
time, at `f7d4fc0f`: step 0 (`linux/w2-s0`, the movie's code in
`src/game/fmv_play.cpp`), R7 (`linux/w2-r7`, `os::OwnStack` and the
snapshot's files) and R6 (`linux/w2-r6`, the window on SDL3, the Win32
window behind `BOF3X_OFF=WindowSdl`). Each group's hosted shadow suite
passed in its own worktree (922 lines of 0); both presets build on the
merge; the merge's own suite and the routes have not run. Running: R5, R6m,
I1 (`input_script.cpp`), R9, M1 (`mp3.cpp`'s stack switch, wrong under the
System V convention - R7's finding).

**The owner's look at the SDL3 window, 2026-11-18, the 32-bit build of
`f7d4fc0f` from `game\`:** all eight as expected - a windowed start with
its placement, icon and title; a few minutes of field and battle; a
quicksave and a quickload; F8 to borderless and back; a resize from a
corner and a side; focus away and back; a drag to the second monitor and
back, kept across a restart; the close button and Alt+F4. **Not looked at
yet:** the movies and a close during one (R6m is not merged), the same
steps under `BOF3X_OFF=WindowSdl`, the 64-bit build, a monitor with another
scale setting, R6's six changes one by one (its hand-back lists them; the
ledger entries are owed at the merge).

**Merged since, at `faf9c7c8` (both presets build; the merge's own suite
has not run):** M1 (`linux/w2-m1`) and R9 (`linux/w2-r9`).

- **M1:** `mp3.cpp`'s decoder runs on `os::OwnStack`; the hand switch is
  gone. The self-test now decodes one track on the caller's stack and on
  the own stack and fails unless every sample is the same (782 frames, the
  same). The old x86-64 switch would have written the decoded frame over
  the caller's stack under the System V convention. A call costs 32 to
  56 ns where it cost 2 (about 38 calls a second). No ledger entry: no
  change is intended. What is left of risk: the decoder now has the own
  stack's MXCSR (0x1F80) where it had the caller's; they are equal today
  and nothing in the source sets it.
- **R9:** `render_d3d11.cpp` and `satpixie.cpp` compile for
  `x86_64-linux-gnu` against DXVK-native's headers (a compile alone; no
  link, no Linux GPU). Of the sites call 1 listed, wave 1 and R7 had left
  two `GetClientRect` calls. The window goes through
  `src/render/render_window.h` (`SwapChainWindow`, `WindowClientSize`):
  Windows defines them, stage LC defines them from the SDL window.
  `BOF3X_PIXEL_OFFSET` is a `Fatal` off Windows. `cmake/dxvk_headers.cmake`
  fetches DXVK at commit `b1a1c99a` (the v3.1.1 tag's) with its
  `include/native/directx` submodule, for a target that is not Windows
  alone; `linux_tree.py --dxvk DIR` points the probe at the headers, and
  the census is regenerated with them present.
- **A correction to the staging's words:** "Windows keeps Microsoft's
  headers" - on this toolchain the Windows builds compile against
  llvm-mingw's MinGW-w64 headers.
- **The fetched DirectX headers' licence, the owner's answer
  (2026-11-18):** the submodule is MinGW-w64's DirectX headers (LGPL 2.1
  or later). They are part of the toolchain's kind of header, the Windows
  builds already compile against the same family, and nothing is vendored:
  it is fine. A notice for DXVK (zlib) in
  [`THIRD_PARTY.md`](THIRD_PARTY.md) is owed when a tarball ships its
  libraries (stage LD).

**All eight groups merged, `98ae6e11` (2026-11-18).** R6m
(`linux/w2-r6m`), R5 (`linux/w2-r5`) and I1 (`linux/w2-i1`) after the five
above. Both presets build; `data_census --strict`, `reloc_census --check`,
`reloc_bare --check`, `ledger_check.py` and `ledger_switches.py --check`
pass. **`linux_tree.py` with DXVK-native's headers: 813 files, 813 compile
for `x86_64-linux-gnu`, 0 do not; 11 are Windows-only by design.** The
round's goal is met. Nothing links or runs on Linux.

- **R6m:** on SDL's window the movie has no subclass and no Win32 pump; it
  takes the window's events (`game_window::TakeForMovie`). The picture on
  Windows is the same GDI code, in `fmv_play_win32.cpp`. Off Windows the
  picture is a `Fatal` that names the open decision
  ([`replacing-mci.md`](replacing-mci.md) 9: the renderer brought up before
  the movies, or SDL's window surface; neither built).
- **R5:** `src/input/key_sdl.*`, 159 pairs from SDL's scancode to the DIK
  number; SDL's keyboard is read at the latch. DirectInput is left under
  `BOF3X_OFF=WindowSdl` and `BOF3X_OFF=KeyboardSdl` (the proof tool: it logs
  each key the two readers keep apart). `tools/key_host`: 132,756 checks, 0
  failed, at both widths and in the WSL. Read, not run: the state hash's
  page `0x7DE000` will differ from a reference made before R5
  (`DInput_Object` and `DInput_Keyboard` stay 0).
- **I1:** `input_script.cpp` has no Win32 site; on SDL's window a recipe's
  key goes into the window's queue (`game_window::QueueKey`), on the Win32
  window through `os::PostWindowKey`. None of the ten routes has a key
  step; the recipes with one are listed in its hand-back (`quick_menu`,
  `f9ExitMode` and others).
- **Found, not of this round:** the launcher's key capture stores Pause as
  0x45 and names it NumLock, and the reverse
  (`config_dialog.cpp`, the `WH_KEYBOARD` hook); R5 kept DirectInput's
  meaning so that no stored number changes.

**The owner's second look, 2026-11-18, the 32-bit build of `f32becb3`
(`launcher/function-keys`, which holds all of the above) from `game\`:**
rebinding, clearing and a double binding on the launcher's new Function
keys page work, and "the keyboard strokes seem fine" in play. **The
movie** (the owner, the same day: a normal build has one now): it loaded
and played to its end once, and was skipped once. **The function actions**
(the owner, the same day): those bound to a pad button and those on the
function keys "worked great". **Not reported on:** a
close of the window during the movie,
the pad unplugged and plugged in, MP3 music on the own stack, satpixie,
`BOF3X_OFF=WindowSdl` and `BOF3X_OFF=KeyboardSdl`, the 64-bit build.

**The long proofs on the merged branch, 2026-11-18** (`94307c45`, 32-bit,
clean launcher folders under `C:\Users\kerok\bof3x-runs\lbw2\coord`, the
machine hands-off, no compiler seen during any run; the chain is
`proofs.py` there):

- **Self-tests:** hosted with `BOF3X_SHADOW='*'` - `self-test only: done`,
  922 `MISMATCHES` lines, every one 0, 0 FATAL; the exe host - exit code 0,
  10,392 log lines, 0 FATAL; the exe host at `BOF3X_IMAGEBASE=0xA00000` -
  the same three figures, the image moved from `0x400000`.
- **The ten routes** (the exe host, ours as `cn_<route>_w2`): `diff`
  against `_w1` and against `_m1116` is identical on every compared tick on
  the seven routes of wave 1's list; the other three differ on `0x6C3000`
  alone, over wave 1's ticks (its run noise). `check` against the `orig5`
  pair gives `m1116`'s lines on every route. Page `0x7DE000`, which the
  read pass expected the keyboard to change, did not change.
- **whelpBoss's length is a race at the close, not a difference:** the hash
  file is written 64 ticks at a time and the recipe ends at frame 13,181.
  Of three runs of this build two ended with 13,121 ticks (`_w2`, `_w2b`)
  and one with 13,185 (`_w2c`, identical to `_w1` on all of them). Whether
  the SDL window closes sooner than the old one was not measured.
- **Key steps on both paths** (`quick_menu`, `f9ExitMode`,
  `quick_combat_keys` under `en-US`; `field_f9`; a harness game folder):
  each ran to `done` on the SDL window and keyboard, with
  `BOF3X_OFF=WindowSdl` and with `BOF3X_OFF=KeyboardSdl`, and the state
  hash of the three is identical on every tick (961; 313; 2,625; 4,673).
- **`music_heard.py`:** pass on whelpBoss (164 ring lines, 6 tracks) and
  cutsceneAndNue (116 ring lines, 5 tracks).
- **`BOF3X_ARENA=1`:** combat and cutsceneAndNue ran to `done`, identical
  to `_w2` on every tick (2,561; 7,425).

**Not run:** anything at 64 bits beyond the build; a route with
`BOF3X_OFF=WindowSdl` other than the four key recipes.

**After the proofs, step 0 undone (CI's source-layout gate, 2026-11-18).**
Step 0 broke a rule the wave's gate line did not run: every function of
Capcom's is in its unit's file (`tu_census.py --check --strict`,
`retire_shells.py --check --strict`). `Fmv_Play` and `Fmv_WndProc` are
`battle_menu_win.cpp`'s. The portable player (R6m left it with no Windows
header) is back in that file, text unchanged; `fmv_play.cpp` and
`fmv_play.h` are gone; the Win32 half is
`src/game/platform/fmv_play_win32.cpp`, and `Fmv_WndProc` in the unit's
file calls its `fmv_play::native::WndProc` on Windows and stops the game
elsewhere, as before. Read "fmv_play.cpp" above as that code. On the
change: both presets build; the 26 steps of CI's "Ledgers and symbols" job
pass locally; `linux_tree.py` 812 of 812 (one file fewer), 11 Windows-only;
the hosted shadow suite and the exe host as in the list above. The long
proofs were not run again, and the movie was not looked at again. **A
later wave's gate line must hold those two checks.**

### The review of the round's three PRs (2026-11-18, after the merges; branch `launcher/game-runs-sdl-class`)

PRs #259, #261 and #262 were read after they merged: #259's changes that
are more than a type's name (104 of them) and the first half of #261 in one
sitting, the rest of #261 and all of #262 by three readers. Not read line
by line: the 5,077 `long` sites of #259. Read and found sound: the heap
arena, the crash catcher's two halves, the log's writer, the 159 key pairs
against SDL's and DirectInput's headers, `OwnStack` on both systems, and
each message the port's window procedure handled.

What it found, each fixed on this branch (known-defects D428..D431; the
ledger's DIV-0238 and DIV-0239 amended):

| Finding | Where | A Windows player today? |
|---|---|---|
| The save editor did not see a running game: SDL's window had SDL's class, not `Bof3` | `game_window_win32.cpp`, `save_dialog.cpp` | yes (D428) |
| The pad's events stayed in SDL's queue while `Pad_Read` did not run (F9, the quick menu, not in front) | `game_window.cpp` `FlushSdl` | yes, after a very long pause |
| Refusal messages quoted a number that was not the failed call's | `snapshot.cpp`, `wav_dump.cpp`, `os/win32/out_file.cpp` | the text only (D430) |
| Off Windows, Alt, F10 and Alt+key reached `WinGame_Key` | `game_window.cpp`, `key_sdl.h` | no |
| F11's frame save split its path on a backslash and said nothing when there was none | `magic_080.cpp` | no |
| `strtol` / `strtoul` results narrowed before their range checks | 24 files, `common/strto32.h` | no (D429) |
| The POSIX event, the named event's clock, `MoveFileKeep` with no hard links | `src/os/posix` | no (D431) |
| `gen_symbols.py` rewrote the second word of `long long` | `tools/gen_symbols.py` | no (no such row today) |

Also written down, not changed: AltGr under SDL's keyboard is the right Alt
alone (DIV-0239's list; what DirectInput did is not measured - **the
owner's**, with an AltGr layout and `BOF3X_OFF=KeyboardSdl`, one press and
a look at the log).

Run on the merged branch: see the PR. Not run by anyone: a physical pad,
a filesystem without hard links, a wall-clock step, the new message texts'
failure paths, and anything on Linux beyond compiling and the POSIX
`os_tests`.

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

### Stages LC and LD, staged as one round (2026-11-19; not launched)

**Staged, not started.** It launches at the owner's word and after the calls
under "For the owner, before a launch" below. Stage LB is done: 813 of 813
source files compile for `x86_64-linux-gnu` and 11 are Windows files by
design ("Wave 2 as run"). Nothing links. This section cuts what is left into
groups, as the 38 files were cut. Measured at `main` `8cd45199` by the
command beside each figure; what was not measured is at the section's end.

**Goal:** a `bof3x` ELF that starts from a player's settings file, plays an
install, and passes stage LC's gate; then a CI job that keeps it building and
a tarball a Linux player can use.

**What was found for this staging, beyond the two stage lists above:**

- **Nothing on Linux reads `bof3x.ini`.** The engine reads `BOF3X_`
  environment switches and never the file: `grep -rn "bof3x.ini" src/hook
  src/game src/common` finds comments and message texts alone. The launcher
  turns the file into the environment (`ConfigApplyEnvironment`,
  `src/launcher/config.h:302`; 32 `SetEnv` sites in `config.cpp`) and writes
  the game's own configuration (`ConfigApplyGameCfg`, `:327`). `linux_tree.py`
  skips `src/launcher/` (`SKIP`), so that code has not been compiled for
  Linux. Section 5's stand-in 2 ("the `.ini` is already the contract") needs
  a reader on Linux: group C7.
- **The seam has no call that starts a process** (`ls src/os/*.h`: 23
  headers, none for it; `grep -niE "spawn|CreateProcess" src/os/*.h` finds
  nothing). The launcher's five `CreateProcessW` sites start the game, the
  developer launcher and the importer.
- **The movie's picture is a `Fatal` off Windows**
  (`battle_menu_win.cpp:1053`, the `#ifndef _WIN32` half), and the decision
  between the two ways is open ([`replacing-mci.md`](replacing-mci.md) 9). A
  start plays the logo movie before the title when the install has one.
  Group C5, and call 2.
- **Unit LB8 did not run.** No case rule is in the POSIX backend (`grep -rniE
  "casefold|strcasecmp|tolower" src/os/posix` finds nothing), and LA5 is not
  in "Stage LA as run". The home folder is the executable's folder or
  `BOF3X_HOME` (`src/common/home.cpp:20`), built with backslashes
  (`home + L"\\game"`). Group C4, and call 3.
- **No Windows 64-bit state-hash reference is recorded.** LC's gate compares
  Linux with "the Windows 64-bit references"; "Wave 2 as run" says "Not run:
  anything at 64 bits beyond the build", and PR #229's record says the same
  of the 64-bit routes. Group C0 makes them first.
- **No build list makes a Linux target.** `CMakePresets.json` has `i686` and
  `x86_64`, both llvm-mingw; `CMakeLists.txt:193` says so in a comment.
  `cmake/dxvk_headers.cmake` fetches DXVK's headers and builds no library.
- **The shaders need no compiler on Linux:** `src/render/shaders_compiled.h`
  is tracked (`git ls-files`), and `tools/compile_shaders.py`, which makes
  it, is one of the nine tools that use `ctypes` (the list is under C8).
- **This machine's WSL can run a first Linux start.** Ubuntu 26.04.1,
  kernel 6.18.58.1, `mmap_min_addr` 65,536, `g++-15`, `meson` and `ninja`;
  `mesa-vulkan-drivers` 26.0.8 with `libvulkan_lvp.so` (a software Vulkan)
  and `/dev/dxg`; no `clang++` and no `cmake` on its path (`wsl -e bash -lc`,
  2026-11-19). Section 2a ran tier 1 there on a software renderer. The state
  hash does not depend on the picture, so the routes can be proven here; a
  GPU, the sound by ear and a pad are frost's machine or a tester's.

| Group | What it does | Files | Proof |
|---|---|---|---|
| C0, the 64-bit oracle | The ten routes, `music_heard.py` and the exe-host self-test on the Windows 64-bit build of `main`, kept as the references LC compares with. A difference from the 32-bit references is read and named before anything else starts | no source; `analysis/statehash` | `statehash.py diff` against `orig5`'s pairs with the 64-bit round's allowance |
| C1, the Linux target | A `linux-x86_64` preset: clang, C++20, non-PIE, linked high (`-Wl,-Ttext-segment`, LA2), `__cdecl` as `ms_abi` (LA3); the source list without `linux_tree.py`'s `WINDOWS_ONLY`, `src/launcher/` and `src/os/win32/`; SDL3 as today; DXVK-native's two libraries built pinned at the commit `cmake/dxvk_headers.cmake` names (`meson -Dnative_sdl3=enabled`). First aim: every object, then the list of undefined names at the link, which is the measure of C2 to C6 | `CMakePresets.json`, `CMakeLists.txt`, `cmake/` | The link's undefined names written to `analysis/linux_link.tsv`; both Windows presets still build with the same objects (`codegen_same.py`) |
| C2, the start-up | A `main` that does `Engine_Main`'s steps in its order without a PE loader ("R3: what a Linux start-up needs from the PE host"): reserve the span, map the image at `0x400000` or the moved base, `.stub` no-access, `.data` from `ExeImage_Source` or the player's `BOF3.exe`, the pointer words, the import slots, the x87 word, `Crash_Start`, `InjectAll`, `Game_WinMain`. `host.cpp` split into its portable part and a per-host part; `ImageBase()` given a source that is not the process environment block; `ExeFile_Present` / `ExeFile_Read` as portable code over the PE file's section table | new `src/hook/exe_host_posix.cpp`, `host.cpp`, `src/common/image_base.h`, `exe_host.cpp` | On Windows: the hosted suite, the exe host and the moved base as before. On Linux: `BOF3X_SELFTEST_ONLY=1` to `self-test only: done` |
| C3, memory below 4 GB | The reservations made before anything else allocates, in one written order: the image, the task stacks at their Windows place, the arena (R10, the default on Linux, no switch), the draw pool. A refusal that names the range when a reservation fails. The task switch under `ms_abi` run for the first time | `src/os/posix`, `sound_ours.cpp`'s arena, `task.cpp` | `lowmem_probe`'s cases as tests in `tools/os_host`; the self-test's task cases on Linux |
| C4, paths | LA5 and LB8: the case of every name the engine builds against what an install holds, fixed at its source or folded in one place in the POSIX backend (call 3); backslashes in the names `home.cpp` and the save code build; where the home, the saves, the log and the crash report go (call 3); D431's open item, the zero-byte slot file after a crash in `MoveFileKeep`'s third way | `src/os/posix`, `src/common/home.cpp`, the path builders LA5 lists | A Windows run with a case-checking open in the seam (LB8's proof); `os_tests` for each new call; a quicksave made and loaded on Linux |
| C5, the picture | The renderer's window from SDL's (`SwapChainWindow`, `WindowClientSize`, `src/render/render_window.h`) with DXVK-native taking SDL's window as its `HWND`; the window icon by `SDL_SetWindowIcon` from `exe_icon.cpp`'s portable picture code; the movie's picture off Windows by the way call 2 chooses | `src/render`, `game_window_other.cpp`, `battle_menu_win.cpp`, a portable part of `exe_icon.cpp` | A frame saved by the game's own readback (`BOF3X_SHOT_DIR`) on Linux against the Windows 64-bit frame of the same tick, point filter, pixel for pixel; the logo movie's frames against the decoder's dump |
| C6, faults and the watch | `crash_posix.cpp` in a real run: a crash on purpose (`BOF3X_CRASH_TEST`) read by `tools/crash_report.py`; `stackwatch_posix.cpp` refuses today, so either a guard page for the main stack or the refusal kept and said | `src/hook/crash_posix.cpp`, `stackwatch_posix.cpp` | `tools/linux_probe/crash_catch_test.sh`; a `CRASH n:` report from the ELF |
| C7, the settings without a window | `config.cpp`, `suites.cpp` and `exe_image.cpp` off `<windows.h>` (164, 1 and 30 census sites: `python tools/os_census.py --files src/launcher`), a process-start call in the seam, and a `bof3x-launcher` for Linux with no window: it reads `bof3x.ini`, applies the environment and the game's configuration as the Windows launcher does, and starts `bof3x`. It is stage P0 of [`launcher-portable.md`](launcher-portable.md), and it is needed here whatever that document's answer is | `src/launcher/config.cpp`, `suites.cpp`, `exe_image.cpp`, a new `src/os/process.h`, `launcher.cpp`'s start half | `config_tests` (`tools/launcher_host`) on Windows and in the WSL; the environment the two launchers set from one `bof3x.ini`, compared line by line |
| C8, the harness | A runner for Linux that needs no window control: a recipe by the `BOF3X_` switches, the state hash to a file, the exit code. `input_run.py` and `abc_run.py` take a `--host linux` that starts the ELF (in the WSL from Windows, or directly on Linux). The nine tools that use `ctypes` (`grep -rlE "import (ctypes\|winreg\|win32\|msvcrt)\|ctypes\.windll\|from ctypes" tools`: `close_probe.py`, `compile_shaders.py`, `ini_probe.py`, `key_probe.py`, `proc_image.py`, `revisit_diff.py` and three under `launcher_host/`) stay Windows tools, each saying so when run elsewhere | `tools/` | One route run twice on Linux, identical but for the pages two Windows runs also differ on |
| C9, the install | The importer under Linux's Python with a requirements file (call 3 of section 6; no `requirements*` file is tracked today); the case of the names it writes checked against C4's rule; an install made on Windows and copied, checked by `verify` | `tools/importer.py`, a new requirements file | `setup` on Linux from the US disc, then `verify`; the cache's manifest equal to a Windows-made one |
| D1, CI | A native job on `ubuntu-24.04`: the Linux preset builds and links, `os_tests`, `config_tests`, the self-test with no game data if one exists (as `os-host` runs today); the header ratchet of section 3 (no game folder includes an operating-system header) | `.github/workflows/build.yml`, `checks.yml` | The job green on the round's PR |
| D2, the release | `tools/make_release.py` makes the tarball of call 5: the ELF, C7's launcher, DXVK-native's two libraries, the importer, a start script. DXVK's notice in [`THIRD_PARTY.md`](THIRD_PARTY.md) (owed since wave 2). The README's Linux section and the release notes | `tools/make_release.py`, `release.yml`, `docs/THIRD_PARTY.md`, `README.md` | The tarball unpacked on a clean machine or container: `setup`, then the title screen |
| D3, the people's checks | Tier 1's rows 5, 7 and 9 to 12 under Proton (2c); the tier-1 checklist's rows 4 to 10 on the native build by a Linux tester; the owner's looks still owed from waves 1 and 2 (below) | none | The checklist's table filled in section 2c's form |
| D4, the records | [`STATUS.md`](STATUS.md) and [`HANDOFF.md`](HANDOFF.md) rewritten for the round: HANDOFF's Linux lines still say wave 2 is not launched and name a second backend as the gate (`HANDOFF.md`, "The Linux round's next units"), which call 1's answer retired | docs | `ledger_check.py`; the docs' own checks in CI |

**Three waves. One round runs at a time; the first two need no eye.**

- **Wave A, on Windows alone, nothing a player can tell:** C0 first and by
  itself (it is game runs on a quiet machine). Then, side by side: C1's
  preset and list, C2's split of `host.cpp` and the portable `ExeFile_Read`,
  C4's Windows half (the case-checking open and the fixes it finds), C7's
  Windows half (`config.cpp` on the seam, `os/process.h`), C8's runner, C9's
  requirements file. Each is proven by the Windows gate line.
- **Wave B, the first link and the first run, in the WSL:** C1's link to
  zero undefined names, then C2, C3, C5 and C6 in that order on one branch,
  since each is found by the run stopping at the next thing; C4's and C7's
  Linux halves beside them. It ends at LC's gate: the ten routes against
  C0's references, `music_heard.py`, a quicksave made and loaded on Linux.
  One agent at a time owns the run; the others hand it patches.
- **Wave C, what a person must see:** D1 and D2 by agents; D3 by frost, a
  tester and the owner; D4 last.

**The gate line, each group, before its hand-back.** The Windows line of
"The 38 files, staged as one round", and with it the two checks that wave 2
missed (`tu_census.py --check --strict`, `retire_shells.py --check
--strict`). From wave B on, also:

- the Linux preset builds and links with no undefined name;
- `os_tests` and `config_tests` in the WSL;
- `BOF3X_SELFTEST_ONLY=1` on the ELF to `self-test only: done`, 0 FATAL;
- for C3 to C6, which change what runs: the route the group names, against
  C0's reference.

**What two groups share, and how it merges.** `CMakeLists.txt`'s source
lists and `src/os/os.h`, as before: keep both sides. The censuses are
regenerated by the coordinator after each merge. Agents number nothing in
the ledger or the defect list. C2 and C3 both touch the start-up's order:
C3 writes the order as a table in this file first, and C2 codes to it.

**For the owner, before a launch:**

1. **Who runs it.** Wave A needs no Linux machine. Wave B can run in this
   machine's WSL on a software Vulkan, or on frost's machine. The owner's
   answer of 2026-11-18 was "wave 2 runs here; frost takes the round over
   when he is ready". Proposed: waves A and B here, wave C's GPU, sound and
   pad rows his; ask him before wave B starts, so that two people do not
   write the same start-up.
   **Decided (the owner, 2026-11-19): what can be done here is done here**,
   and frost takes it up when he has time. Waves A and B run here; wave C's
   GPU, sound and pad rows stay his or a tester's.
2. **The movie's picture off Windows** ([`replacing-mci.md`](replacing-mci.md)
   9). Proposed: the renderer brought up before the movies, since SDL3 will
   not combine its window surface with a 3D API on one window and the
   renderer is needed a moment later anyway. It changes the Windows order
   too if both systems are to share one path; then it is a ledger entry and
   an owner's look at the three movies.
   **Decided (the owner, 2026-11-19): as proposed** - the renderer is brought
   up before the movies.
3. **Where a Linux install keeps its files, and the case rule.** Proposed:
   the tarball stays a portable folder as on Windows (the home is the
   executable's folder, `BOF3X_HOME` overrides), so one rule serves both
   systems and the Deck; XDG folders are a later call. For case: fix each
   name at its source where the engine builds it, and fold case in the
   POSIX backend only for names the player's own files bring (a disc's
   folder, a mod's zip).
   **Decided (the owner, 2026-11-19): as proposed** - a portable folder, and
   names fixed at their source.
4. **The first native release's front end.** Proposed: C7's launcher with
   no window plus a text file, as call 5 decided; the window is
   [`launcher-portable.md`](launcher-portable.md)'s question and does not
   gate this round.
   **Decided (the owner, 2026-11-19): as proposed for the interim** - a
   launcher with no window and the text file. The owner's instinct is that
   the launcher's portable work can follow quickly; its toolkit is call 1 of
   [`launcher-portable.md`](launcher-portable.md) section 7, not yet answered.
5. **Whether C0's references need a hands-off machine now.** Ten routes on
   the 64-bit build is about the length of one proof chain; nothing else in
   wave A waits for it but wave B's gate.
   **Decided (the owner, 2026-11-19): the evening of 2026-11-19.**

**Still owed from waves 1 and 2, the owner's (D3 collects them):** AltGr
under SDL's keyboard with `BOF3X_OFF=KeyboardSdl` and a look at the log; a
physical pad after the review's `FlushSdl` fix, unplugged and plugged in;
the save list opened once on the 64-bit Windows build; the movie and the
long proofs after step 0 was undone; a close of the window during a movie;
MP3 music on the own stack; satpixie; `BOF3X_OFF=WindowSdl` outside the four
key recipes.

**Not measured for this staging:** the size of the linked ELF and so
whether a high link is needed or merely safe; the number of undefined names
at the first link (C1 measures it); whether DXVK-native builds in this
machine's WSL (frost built v3.1.1 on CachyOS) and whether `lvp` or the
`/dev/dxg` driver is what Vulkan picks there; whether the software Vulkan
is fast enough for a route to end in the time its recipe expects; the
launcher's `config.cpp` under a 4-byte `wchar_t`; a whole-tree link under
`ms_abi` and a call across the SDL or DXVK boundary (section 7); SteamOS's
`mmap_min_addr`; whether the Windows 64-bit build's routes equal the 32-bit
ones today (C0).

### Wave A as run (2026-11-19, branch `linux/lcA`, not merged to `main`)

Launched at the owner's word, six groups side by side in worktrees from
`75aa7b53`, each merged `--no-ff` into `linux/lcA` when it handed back: C9,
C8, C1, C4, C7, C2. One conflict, in `tools/os_host/os_tests.cpp`, where C4
and C7 each added a block: both kept. C0, the 64-bit references, is not run
yet (the owner: the evening of 2026-11-19).

**The measure after the merge** (`33de9f1b`): `linux_tree.py` 831 files, 831
compile for `x86_64-linux-gnu`, 0 do not, 11 Windows-only. The count rose from
814 because the launcher's model, `start.cpp`, `headless.cpp`, `save/` and the
new portable files are compiled now (the staging's 813 was stale by one: 825
candidates less 11 at `75aa7b53`).

**C1, the first link.** A preset `linux-x86_64` and `cmake/linux_target.cmake`;
CMake and `linux_tree.py` read one source list (`--cmake-list`). Cross-compiled
on Windows against a sysroot copied out of the WSL (`cmake/linux_sysroot.sh`):
**1,052 objects, 0 errors; the link with `ld.lld` is 14 names short**
(`tools/linux_probe/linux_link.py`): 12 of ours not yet written (`bof3::Host`,
`CodeLo`, `CodeHi`, `Bof3_ImageMove`, `ExeFile_Present`, `ExeFile_Read`, `main`,
`ExeIcon_Frame`, `GapWatch_Pause`, `GapWatch_Tick`, `render::SwapChainWindow`,
`render::WindowClientSize`), the PE linker's `__ImageBase`, and DXVK's
`D3D11CreateDeviceAndSwapChain`. C2 has since written six of the twelve; the
link was not measured again after the merge. Linked with the undefined names
ignored, the ELF spans `0x10000000` to about `0x12708948` (text 12.7 MB, bss
28.1 MB): **the high link is needed, not merely safe.** `ld.lld` refuses
`-Ttext-segment`; the build passes `--image-base` to it.
- **Found: DXVK-native opens SDL3 by name** (`LoadLibraryA("libSDL3.so.0")`,
  its `src/wsi/sdl3/wsi_platform_sdl3.cpp:12-20` at `b1a1c99a`). With SDL3
  static in the ELF it would load a second copy, and our window would be the
  other copy's. Wave B: SDL3 shared off Windows, `$ORIGIN` as RUNPATH,
  `libSDL3.so.0` beside the ELF. Call 5's tarball then holds three libraries.
- The Windows builds: every object and binary of both presets hashed equal
  to the base's (C1's `objhash.py`).

**C2, the host.** `host.cpp` is portable (`SetHost`, the checks, the state);
`DetectHost` and `VerifyImage` are in `host_win32.cpp`, which takes
`host.cpp`'s place in `WINDOWS_ONLY`. `src/hook/exe_file.cpp` reads the
player's `BOF3.exe` through the seam by the PE format's offsets; the 12 call
lines are unchanged. `ImageBase()` is the same code on Windows
(`codegen_same.py --by-function`: 32 symbols differ at 32 bits and 30 at 64,
every one in the four files changed) and reads a variable the start-up sets
off Windows, a `Fatal` if read first. `src/hook/exe_host_steps.h` lists
`Engine_Main`'s 25 steps with what each needs of Windows: wave B's work order.
- **A Windows difference a player cannot see:** `BOF3.exe` is opened without
  denying writers, where `CreateFileW` with `FILE_SHARE_READ` denied them.
- In "R3: what a Linux start-up needs from the PE host", read `host_win32.cpp`
  for `DetectHost` and `VerifyImage`, `magic_080.cpp:668` for `:803`, and
  `rdata_consts.cpp:46` and `:128` for `:37` and `:109`.

**C4, the case of the paths (LA5, LB8).** `BOF3X_CASECHECK=1` (off by
default) logs a name that opens only by Windows' case folding
(`src/os/case_check.h`; the disk's spelling by `FindFirstFileExW` on each
part, not the final path, which needs an open and resolves links).
`os::OpenNarrow` takes the game's `fopen` and three others, so a backslash is
a separator on POSIX; `home.cpp` joins with the system's separator.
- **Measured from 125 existing logs** (1,575 distinct names, 17,841 opens,
  checked for case against `<main checkout>\bof3`): 1,574 the same; 1
  differs, `SND\OYASUMI.DAT`, on disk `Oyasumi.DAT`.
- **By reading:** nine kind-0 `SND` streams, `BGM\000.dat` and the two movies
  (`capcom.avi`, `logos.avi`) differ in case, each a name in Capcom's data
  against a file of the player's PC disc. None is ours to fix at its source;
  by call 3 they are the POSIX fold's (relative names in the game folder
  alone), or the importer lays them out in upper case. The cache's names all
  agree (two caches listed).
- **D431's open item, proposed and not made:** remove a zero-byte
  `BISLPS1x.DAT` at the target in `snapshot.cpp`'s `Rotate`; the POSIX-only
  form would break the seam's own rule (`os_tests.cpp`, "An empty file at the
  target is a file: refused too").
- Its one game run, the hosted suite with the switch on, opened no misspelled
  game file (one line, from a self-test's own pattern). The routes with the
  switch are not run.

**C7, the settings without a window** (stage P0 of
[`launcher-portable.md`](launcher-portable.md)). `config.cpp`, `suites.cpp` and
the launcher's `exe_image.cpp` take nothing from `<windows.h>` (164, 1 and 30
sites, now 0). The seam has `os/process.h` (a child with an environment, a
working folder, pipes, a wait), `os/wide_file.h` and `os/wide_text.h`.
`launcher.cpp` is split: the steps with no window are `start.cpp`; a launcher
with no window is `headless.cpp` (`--set key=value`, `--list`), compiled for
Linux and not linked.
- **The two Windows launchers, measured the same:** the base's and the
  branch's player's launcher, one `bof3x.ini`, a stand-in game that writes
  down what it is given, five runs: environment (120 or 121 lines), command
  line, working folder, output, `BOF3.CFG` and the ini afterwards
  byte-identical but for the pid. The player's launcher imports none of
  `VirtualAllocEx`, `WriteProcessMemory`, `CreateRemoteThread`.
- **Found, not fixed (a Windows change):** `TomlString` decodes `\uXXXX` one
  UTF-16 half at a time; a mod's name beyond the BMP shows as two U+FFFD.
- **Not measured:** a launcher stderr line with a character the C locale
  cannot convert, now one `%ls`.

**C8, the harness.** `input_run.py` and `abc_run.py` take `--host linux
--elf PATH` (`tools/linux_run.py`): the ELF started directly on Linux, or
through `wsl -e` from Windows with the `BOF3X_` paths translated. Shown
against a stand-in (`tools/linux_probe/run_standin.cpp`), no ELF yet.
`statehash.py`, `music_heard.py` and `recipe_shots.py` run under the WSL's
Python 3.14 as they are.
- **The staging's count of Windows tools was short.** Its `grep` missed
  comma-list imports: `grep -rnE "WinDLL|windll|import winreg|msvcrt|'tasklist'|'taskkill'|'powershell'|wintypes" tools`
  finds 18 files, not 9. Seven more are Windows tools with no guard yet:
  `attract_run.py`, `attract_watch.py`, `recipe_saves.py`, `mem_dump.py`,
  `task_stacks.py`, `cell_peek.py`, `solo_harness.py`.
- **A run that starts the ELF directly has not the launcher's layer** (the
  ini and its defaults). Wave B's runs go through C7's launcher, or the
  Linux and Windows sides of a route differ in environment.

**C9, the importer.** `tools/requirements.txt` (pillow 12.0.0, capstone
5.0.9: the only third-party modules on a player's path) and
`tools/requirements-extra.txt`. Under the WSL's Python 3.14: `identify
--json` on eight paths with no game data, `importer.py check` and `mods.py
selftest` as on Windows. `in_any_case` finds a PC folder's names in another
case. No `setup` was run and no disc read.
- **numpy** is used by `at3.py`'s recipe generator alone and has no wheel
  for Python 3.14: whether a release still carries it is the owner's.
- **Found, not changed:** the mods' text build in "game" mode reads the
  player's folder without the CD pressing's repairs that set-up applies.

**Proven on the merged branch** (`33de9f1b`, 2026-11-19): both presets build;
`data_census --strict`, `reloc_census --check`, `reloc_bare --check`,
`width_census --check`, `ledger_check.py`, `ledger_switches.py --check`,
`tu_census.py --check --strict`, `retire_shells.py --check --strict`,
`os_census.py --check` (3,520 sites) and `--check --boundary`, `linux_tree.py
--check` pass; in the WSL `os_tests` 518 checks and `config_tests` 4,220, 0
failed (C7 had handed back three failures there, all `home.cpp`'s backslash,
which C4 fixed); the hosted shadow suite `self-test only: done`, 922
`MISMATCHES` lines, every one 0, 0 FATAL; the exe host and the moved base,
10,392 log lines each, 0 FATAL.

**C0, the Windows 64-bit references** (the same day, hands off, at `75aa7b53`,
builds `build` and `build-x86_64` of the staging worktree; root
`C:\Users\kerok\bof3x-runs\lcA\coord`, script `c0.py`). The 64-bit exe host's
self-test: `done`, 10,392 log lines, 0 FATAL. Its moved base (`0xA00000`, the
mixed copy of [`x64-build-round.md`](x64-build-round.md) stage D: the 32-bit
launcher pair beside the 64-bit exe, DLL and moved exe): the same, the fence
0x58F000 bytes reserved, 0 held. The ten routes at 64 bits
(`cn_<route>_c0x64.sh`) and at 32 bits the same day (`cn_<route>_c0x32.sh`),
no retry: `combat`, `cutsceneAndNue`, `dragonTransform`, `field_menu`,
`menu_screens`, `shop` and `whelpBoss` identical on every tick, 32 against 64
and 32 against wave 2's `_w2`; `caughFish`, `masterAndManillo` and
`worldMapAndAreaTransition` differ on one page, `0x6C3000`, Direct3D's state,
which differs between two runs of one build on these routes
([`garr-fire.md`](garr-fire.md), [`faerie-fix.md`](faerie-fix.md) 4): here
`_w2`, `_c0x32` and `_c0x64` disagree pairwise on that page from the same
tick. The bytes were not dumped this day. `music_heard.py` at 64 bits:
`whelpBoss` pass (164 ring lines, 6 tracks), `cutsceneAndNue` pass (116, 5).
The first pass of the script ran the moved base through the 64-bit player
launcher, which stops with its "developer launcher's" message; that was the
script's fault and the step ran on the second pass.

**The merged branch's routes** (the same evening, hands off, the build of
`33de9f1b`, `cn_<route>_lcA.sh`, script `merge_routes.py`, no retry):
against `_c0x32` and against `_w2`, the same seven routes identical on every
tick and the same three different on `0x6C3000` alone. `music_heard.py`:
`whelpBoss` pass (164, 6), `cutsceneAndNue` pass (116, 5). A first
`field_menu` run made while the owner used the machine was set aside
(`cn_field_menu_lcA.busy1.sh`) and made again.

**Not run:** the routes with `BOF3X_CASECHECK=1`;
`os_tests` on Windows on the merged branch (each group ran its own);
`dialog_test.py`; anything at 64 bits beyond the build; the link after the
merge. One `os_tests` check (line 1113, the auto-reset event's two waiters)
failed for two groups while builds ran beside it and passed on a quiet
machine, on the base too.

**The WSL since** (the owner, 2026-11-19): clang 21.1.8, lld, cmake 4.2.3,
pkgconf, glslang, and the headers of X11, Wayland, ALSA, PulseAudio,
PipeWire and Vulkan are installed, so wave B can configure and build there
natively. `vulkaninfo` offers one device, `llvmpipe`; section 2a drew at
about 22 frames a second on it at 3440 x 1440 under Proton.

### Wave B, staged (2026-11-20; launched 2026-11-21)

**Launched 2026-11-21 at the owner's word; what ran is in "B1 as run" to
"Wave B as run" below.** As staged: it launches at the owner's word and
after the calls at the end of this section. Base: `main` `e9c1fb41` (wave A, PR #265;
the drawn launcher's cutover, PR #267).

**What changed since the staging of 2026-11-19:**

- **C7's Linux half is done by another round.** [`launcher-portable.md`](launcher-portable.md)
  P0 to P6 put `bof3x-launcher` on Linux: it builds in the WSL
  (`cmake --preset linux-x86_64 -DSDL_X11_XTEST=OFF`, target
  `bof3x_launcher`), reads `bof3x.ini`, and takes the Windows launcher's
  command line. Not seen: it starting a real game, since there was no game
  ELF. Wave B's runs go through it (C8's finding: a run that starts the ELF
  directly has not the launcher's layer).
- **The link, measured natively** (the WSL, clang 21.1.8 with GNU `ld.bfd`,
  which takes `-Ttext-segment=0x10000000`; a copy of `e9c1fb41` under
  `~/src/bof3x-lcB-measure`, `cmake --preset linux-x86_64
  -DSDL_X11_XTEST=OFF && cmake --build build-linux-x86_64 -- -k 0`): every
  object compiles, DXVK-native's two libraries build at the pinned commit
  and link, and **the link is 7 names short**, each with the lines that
  want it:

  | Name | Wanted by | Unit |
  |---|---|---|
  | `main` | the C runtime's `_start` | B1 |
  | `__ImageBase` | `image_base.h:158`, `:159`; `crt_rest.cpp:80` | B1 |
  | `bof3::GapWatch_Pause()` | `input_script.cpp:641`, `:653`, `:1094` | B1 |
  | `bof3::GapWatch_Tick()` | `input_script.cpp:643`, `:663`, `:1096` | B1 |
  | `bof3::ExeIcon_Frame(void*)` | `magic_080.cpp:871` | B1 (B2a makes it whole) |
  | `render::SwapChainWindow(void*)` | `render_d3d11.cpp:916`, `:942` | B1 |
  | `render::WindowClientSize(void*, unsigned*, unsigned*)` | `render_d3d11.cpp:841`, `:952` | B1 |

  Wave A's cross measure gave 14; the other seven are written (C2) or
  resolved by DXVK's libraries (`D3D11CreateDeviceAndSwapChain`).
  `__ImageBase` is still wanted although C2 gave `ImageBase()` a variable
  off Windows: two more readers name the PE linker's symbol directly.
- **SDL3 is still static** (`CMakeLists.txt:137..138`, the cache of the
  measure: `SDL_SHARED=OFF`). C1's finding stands: DXVK opens
  `libSDL3.so.0` by name, so a static SDL3 gives two copies. B1 makes it
  shared off Windows.

**Shape.** Wave B finds its work by running, so its first unit runs alone;
three units then run side by side, each on files of its own; the gate is
the coordinator's. Workers are the `worker` agent type, in worktrees, and
build in the WSL's own file system (`~/src/bof3x-lcB-<unit>`; a build under
`/mnt/c` is slow). The round's branch is `linux/lcB`.

| Unit | Groups | What it does | Proof before the hand-back |
|---|---|---|---|
| **B1, the link and the start-up** (alone, first) | C1's end, C2, C3 | SDL3 shared off Windows, `$ORIGIN` as RUNPATH, `libSDL3.so.0` and DXVK's two libraries copied beside the ELF. The seven names: `__ImageBase`'s two readers to the start-up's variable; the gap watch's two entries in `windows_only.cpp` (the watch is Windows only; with its switch unset they do what Windows does with it unset, and the switch is already a `Fatal` there); `SwapChainWindow` and `WindowClientSize` over SDL's window; `ExeIcon_Frame` over SDL (the picture code made portable, or the icon left out and said). Then `exe_host_posix.cpp`'s `main` coding `exe_host_steps.h`'s 25 steps in order, after C3 writes the reservations' order as a table in this file: the image at `0x400000` (or the moved base), `.stub` no-access, the task stacks, the arena, the draw pool, each refused by name when its range is taken | Both Windows presets' objects unchanged but for the files changed (`codegen_same.py --by-function`); the Linux link with no undefined name; `BOF3X_SELFTEST_ONLY=1` on the ELF through the Linux launcher to `self-test only: done`, 0 FATAL; `os_tests` and `config_tests` in the WSL; the Windows gate line |
| **B2a, the picture and the movies** | C5 | The renderer through DXVK-native on SDL's window to the title; the renderer brought up before the movies (the owner's call 2 of 2026-11-19), on both systems if call 1 below says so; the movie's picture off Windows; the window icon whole | A frame by `BOF3X_SHOT_DIR` on Linux (llvmpipe) against the Windows 64-bit frame of the same tick, point filter, pixel for pixel; the logo movie's frames against the decoder's dump; on Windows the ten routes if the start order changed there |
| **B2b, faults** | C6 | `crash_posix.cpp` in a real run (`BOF3X_CRASH_TEST`), its report read by `tools/crash_report.py`; the stack watch's refusal kept and said (call 2) | `tools/linux_probe/crash_catch_test.sh`; a `CRASH n:` report from the ELF |
| **B2c, paths and saves** | C4's Linux half | The POSIX fold for the names the player's files bring, relative names in the game folder alone (the owner's call 3 of 2026-11-19; the dozen Capcom names wave A listed); D431's open item settled one way, the seam's rule kept | A quicksave made and loaded on Linux; `os_tests` for the fold; the routes' first minute on the PC install and on a disc-built one |
| **B3, the gate** (the coordinator's) | LC's gate | The ten routes on the ELF through the Linux launcher against C0's `_c0x64` references; `music_heard.py` on `whelpBoss` and `cutsceneAndNue`; a quicksave made on Linux and loaded on Windows, and the other way; on Windows the ten routes against `_c0x32` | `statehash.py diff` with the 64-bit round's allowance; the three routes that differ on `0x6C3000` between two Windows runs are read the same way here |

**What may run beside what.** B1 touches the start-up, the build and the
render window's two functions; nothing starts beside it. B2a, B2b and B2c
share no file but `src/os/posix` (B2b's signal code, B2c's fold): each adds
its own file there and one line to `os.h`. Merged into `linux/lcB` one at a
time, `--no-ff`, each built and its proof run again after the merge.

**The gate line, from B1 on.** The Windows line of wave A's brief (both
presets, the censuses, `linux_tree.py --check`, `os_tests` three ways, the
hosted shadow suite under the lock), and in the WSL: the Linux preset links
with no undefined name; `os_tests` and `config_tests`; the ELF's
`BOF3X_SELFTEST_ONLY=1` to `self-test only: done`, 0 FATAL. B2a, B2b and B2c
add the route each names.

**Not measured for this staging:** whether a window of the game's in the
WSL shows on the owner's screen (WSLg opens one at `DISPLAY=:0`, as the
launcher's did) and whether SDL's `offscreen` driver gives DXVK a Vulkan
surface, which would let runs leave the screen alone; whether llvmpipe is
fast enough for a route to end in its recipe's time (2a drew at about 22
frames a second at 3440 x 1440, and a route's window is 640 x 480); the
ELF's size once linked whole; anything at run time.

**For the owner, before a launch:**

1. **The start order on Windows.** The renderer comes up before the movies
   on Linux (call 2 of 2026-11-19). Proposed: the same order on Windows, so
   both systems run one path; that is a ledger entry and the owner's look at
   the three movies on Windows after B2a. The other way keeps Windows as it
   is and gives Linux its own order.
   **Answered (the owner, 2026-11-20): no call needed.** Only the Capcom
   movie is kept; the two PC-only movies are both ignored, so the order of
   the movies does not matter. B2a confirms which movie a start plays on
   Linux and what draws it, and changes no order on Windows.
2. **The stack watch on Linux.** Proposed: keep `BOF3X_STACKWATCH=1` a
   refusal there and say so in the switch's line of the ledger. It is a
   developer's switch; a guard page of our own for the main stack is work
   no player sees.
   **Decided (the owner, 2026-11-20): as proposed.**
3. **SDL3 shared on Linux alone.** Proposed: yes; Windows stays static. The
   tarball (call 5 of section 6) then holds three libraries beside the ELF.
   **Decided (the owner, 2026-11-20): as proposed** - the Linux build ships
   its own `libSDL3.so.0`; Windows is unchanged.
4. **The owner's screen.** Proposed: B1 needs no window (the self-test
   ends before one opens). From B2a on, game runs in the WSL use SDL's
   `offscreen` driver if it serves DXVK; if it does not, they open a WSLg
   window, and they run when the owner is away or says the screen is free.
   **Decided (the owner, 2026-11-20): as proposed.**
5. **Where the gate runs.** Proposed: here, on llvmpipe. If a route cannot
   end in time on it, the gate moves to frost's machine and the record says
   so.
   **Decided (the owner, 2026-11-20): as proposed.** With call 3 answered,
   every call is settled; the launch waits for the owner's word ("hold off
   on beginning", 2026-11-20).

### B1 as run (2026-11-21, branch `linux/lcB-b1`, from `27ff3802`)

Launched at the owner's word of 2026-11-21. Unit B1 alone: the link and
the start-up.

**The reservations' order, written before the code** (task 3; group C3's
table, which C2's `main` codes to). Every range below 4 GB that the Linux
ELF needs at a fixed place, or needs below 4 GB, in the order it is made.
Each is made with no-replace semantics (`os::MemReserve` / `os::MemAlloc`
with an address: `MAP_FIXED_NOREPLACE`, the result checked against the
address asked); a refusal is a `Fatal` that names the range and the
mappings of `/proc/self/maps` that hold it. The ELF itself is linked at
`0x10000000` (`-Ttext-segment`, stage LA2) and is the kernel's to map.

| # | Range | Size | Protection | Made by, when |
|---|---|---|---|---|
| 1 | The image's span, `base`..`base + 0x587000` (`0x400000`..`0x987000` at the player's base; `0xA00000`..`0xF87000` moved): Capcom's sections' extents, `exe_layout.h` | `0x587000` | reserved, no access, in one mapping; then inside it: | `exe_host_posix.cpp`'s `.preinit_array` entry (step 1), before any library's or our own initialiser runs; checked and logged at steps 8 to 10 |
| 1a | the header page, `base`..`base + 0x1000` | `0x1000` | read-only, holding the PE header's first fields as `bof3x.exe`'s (the two signatures, the machine, `e_lfanew`, the timestamp); the rest zero. *Changed in the coding:* the table said "no access, nothing reads it"; the first run faulted reading it - `bind.cpp`'s `Mode` tells the exe host by the header's timestamp at the first `Code()` of an initialiser | as row 1 |
| 1b | `.stub`, VA `0x401000`..`0x5C4000` | `0x1C3000` | no access: the trap (step 10 reads it back). Never writable on Linux: nothing writes it on Windows either | as row 1 |
| 1c | `.game`, VA `0x5C4000`..`0x93F000` (`.rdata`, `.data`, `.bss` to Capcom's `.rsrc` end) | `0x37B000` | read-write, zero (step 11 asserts it) | as row 1 |
| 1d | `.pool`, VA `0x93F000`..`0x987000`: DIV-0062's 2,048 draw items | `0x48000` | read-write, zero but for `draw_pool_room.h`'s stamp at its start, which `make_exe.py` writes into the file on Windows | as row 1 |
| 2 | Only at a moved base: the linked span `0x400000`..`0x987000`, the fence (`exe_host.cpp` `Fence`) | `0x587000` | reserved, no access | as row 1 |
| 3 | The main thread's stack at its Windows place, `0x60000`..`0x160000` (x64-build-design.md 7.1: the task slices lie on it, their saved stack pointers are 32-bit words), with a guard page `0x5F000`..`0x60000` under it | `0x100000` + `0x1000` | read-write; the guard no access | as row 1; `main` moves the main thread onto it (`os::RunOnPlacedStack`) before step 3, so every step from `Engine_Main`'s entry on runs there, as on Windows |
| 4 | The bind table of code places, `(top << 24) \| 0x400000`..`(top << 24) \| 0x5C5000` (`0x7C400000`.. first), the last page the import slots' 14-byte thunks | `0x1C5000` | read-write while built, then read-execute | `bind.cpp` `BuildTable`, at the first `Code()` - a namespace-scope initialiser, after row 1; it tries the top bytes `0x7C`, `0x70`, `0x7E`, `0x7B`, `0x7D`, then any `0x20`..`0x7F`, and stops only when none can be had (its own rule, unchanged) |
| 5 | The arena (R10, the default where the C runtime has no exact size), the highest free run of `[2 GB, 4 GB)` that holds it (`0xF0000000`.. expected, as the 64-bit Windows build measured) | `0x10000000` (256 MB) | reserved, committed read-write 64 KB at a time | `heap_arena.cpp` `LowArena::Open`, at the first `Crt_malloc`; then `[64 KB, 2 GB)` if no run above 2 GB holds it |
| 6 | The draw pool | - | - | row 1d: `DrawPool_Reserve` (step 18, `InjectAll`) finds the stamp there, as on Windows |

Not in the table, and why: the C runtime's own heap (`std::malloc`,
`new`) lies after the ELF's `.bss` while it grows by `brk` and above 4 GB
for a block it maps; the users R10 listed (`render_shim.cpp`'s `Alloc`,
`music_seq.cpp`'s `TestMalloc`, `mp3.cpp`'s `new Decoder`) are found by
`Low32`'s `Fatal` when they meet a high block. The program's own stacks
(`os::OwnStack`: the present's, the quicksave's, the MP3 decoder's) are
mapped where the system puts them.

**What was built.**

- **SDL3 shared off Windows** (`CMakeLists.txt`, `BOF3X_SDL`; Windows keeps
  `SDL3::SDL3-static`). `bof3x` and `bof3x-launcher` carry RUNPATH
  `$ORIGIN`; the build copies `libSDL3.so.0`, `libdxvk_d3d11.so.0` and
  `libdxvk_dxgi.so.0` beside them under their sonames
  (`cmake/linux_target.cmake`). `ldd bof3x`: one `libSDL3.so.0`, the one
  beside it, and DXVK's two from the same folder; DXVK's `LoadLibraryA` of
  `libSDL3.so.0` resolves to the object already loaded by that soname (not
  watched at run time: the self-test makes no DXVK device).
- **The seven names.** `crt_rest.cpp`'s `__ImageBase` reader takes the
  start-up's `image::g_ownBase` off Windows (the `image_base.h:158` of the
  staging's table was `Low32`'s template body, where that one use was
  instantiated). `GapWatch_Pause` / `GapWatch_Tick` were already in
  `windows_only.cpp`, which no list held: `tools/linux_probe/linux_tree.py`
  has a `LINUX_ONLY` table now (`exe_host_posix.cpp`, `windows_only.cpp`,
  `exe_icon_sdl.cpp`, `render_window_sdl.cpp`) that `--cmake-list` appends.
  `SwapChainWindow` / `WindowClientSize`: `render_window_sdl.cpp`, the
  `SDL_Window*` itself and `SDL_GetWindowSizeInPixels` (DXVK's `fromHwnd` is
  a cast; its own `getWindowSize` asks the same call). `ExeIcon_Frame` and
  `ExeIcon_SelfTest`: `exe_icon_sdl.cpp`, SDL surfaces and SDL's PNG coder,
  over `exe_icon_picture.cpp`, the picture code moved out of `exe_icon.cpp`.
  `main`: `src/hook/exe_host_posix.cpp`.
- **The start-up.** The reservations of the table above in a
  `.preinit_array` entry, which also calls `SetImageBase`; it reads
  `BOF3X_IMAGEBASE` from the environment block the loader passes it,
  because `getenv` answered "unset" that early (measured). `main` opens the
  log, calls `SetHost`, and moves the main thread onto the stack of row 3
  with the new seam call `os::RunOnPlacedStack` (`src/os/placed.h`;
  `getcontext` / `swapcontext` on POSIX, `CurrentStack` answering the range;
  refused on Windows) to run steps 3 to 25. Step 13's fourteen slots hold
  thunks of functions of ours: `GetTickCount` the monotonic clock;
  `CreateEventA`, `CloseHandle`, `MsgWaitForMultipleObjects` over
  `os/event.h`; `IntersectRect` USER32's rule; `GetDriveTypeA` "no root"
  (no drive letters); `PostQuitMessage` the window queue's `Got::Quit`;
  `MessageBoxA` SDL's message box; `ExitProcess`, `TerminateProcess` the
  process's end; `DirectSoundCreate` and GDI32's three a `Fatal` (step 14
  rebinds the first; the renderer hands out no device context for the
  others). Step 17 by `os/named_event.h`. Step 23 is `crash_posix.cpp`'s:
  its report, then the signal's own end, where Windows ends with code 3.
- **Found on the way.** `os::MemProtect` / `MemRestore` on POSIX passed an
  address that was not page-aligned to `mprotect` (EINVAL at the second
  import thunk); they take whole pages now, as `VirtualProtect` does.

**Measured.**

| Check | Result |
|---|---|
| The Linux link (WSL, clang 21.1.8, GNU ld) | no undefined name; the brief's configure in a clean folder, everything fetched and DXVK-native built by the external project: the same |
| `size bof3x` | text 11,761,344, data 14,032, bss 28,120,785; the ELF spans `0x10000000`..`0x1260E438`, clear of `0x400000`..`0x10000000` |
| Step 1's order | without the `.preinit_array` entry (`SetImageBase` called first thing in `main`) the ELF stops before `main`: `FATAL: ImageBase() read before the start-up set it`; with it, every run below |
| `os_tests` | WSL (cmake, and g++ 15 by the brief's line): 558 checks, 0 failed; Windows i686 634, x86_64 638, 0 failed |
| `config_tests` (WSL) | 4,400 checks, 0 failures (the host build's `save_tests` target fails to compile under g++, the same at `e9c1fb41`) |
| The ELF's self-test through the Linux launcher, the PC install | `BOF3X_EXEFILE=1`: 10,393 log lines, 0 FATAL, `self-test only: done`; `BOF3X_CACHE=analysis/cache/pc-plus-us` (the player's half): 10,400 lines, the same. Without either the launcher refuses ("no game data"), as it does on Windows: `<main checkout>/bof3` has no cache |
| The moved base on Linux (`BOF3X_IMAGEBASE=0xA00000`, the ELF started directly: the Linux launcher refuses a moved base) | 10,392 lines, 0 FATAL; the image at `0xA00000`..`0xF87000`, the linked span fenced whole, the arena at `0xF0000000` |
| The icon's self-test on Linux (`analysis/icon/vram_load.bin`, `whelp_centre.ico`) | 69,696 pixels identical; against a wrong reference, the Fatal |
| Windows: both presets; `codegen_same.py --by-function` against `27ff3802` | 32-bit: 52 symbols differ, all `exe_icon*`; 64-bit: those and 3 in `crt_rest.cpp`, the line numbers `Low32` takes for its message (`__builtin_LINE`) |
| The hosted shadow suite (32-bit), the exe host, the moved base | 922 `MISMATCHES` lines, every one 0, 0 FATAL, `done`; the exe host and the moved base (`BOF3X_EXEFILE=1`): 10,392 lines each, 0 FATAL |

**Not run:** steps 21 to 25 (B2a's); a window; DXVK at run time.

### B2a as run (2026-11-21, branch `linux/lcB-b2a`, from `3080284c`)

Unit B2a: the picture and the movies. Steps 21 to 25 of
`exe_host_posix.cpp` ran for the first time, the logo movie and the
game's frames reached the window through DXVK-native on llvmpipe, and the
game ran from the start to the attract demo (area 4) under SDL's
`offscreen` driver. Game runs went through `./bof3x-launcher --game
<main checkout>/bof3 --no-config` with `BOF3X_EXEFILE=1`, under the round's
lock; the WSL's audio went to `SDL_AUDIO_DRIVER=dummy` after the first two
runs (those two played through WSLg's "RDP Sink").

**DXVK and the `offscreen` driver: a surface, yes.** SDL3's `offscreen`
driver gives DXVK-native a Vulkan surface through `VK_EXT_headless_surface`
(llvmpipe has it, `vulkaninfo`); the swap chain is made, presents, and the
game runs without a window on the screen. Every frame comparison below was
made that way. No headless X server was needed (none is installed:
`Xvfb`, `weston` absent).

**What the start needed, in the order met.**

1. **The movie's picture** (`replacing-mci.md` section 9; the owner's call of
   2026-11-19). Off Windows `fmv_play::native::Draw` was a `Fatal`. Now
   `render::PresentMovie` (`render_d3d11.h`, Linux only) brings the
   device and swap chain up on the game's SDL window at the first movie
   frame - with the present shader, its two samplers and its blend state
   only - and draws each frame: the client cleared black, the frame into
   DIV-0035's rectangle, a whole multiple by the point sampler (pixel for
   pixel), a fit by a mip chain and the linear sampler (an average, where
   Windows' GDI uses HALFTONE). `InitD3d11` from `Display_Setup` then
   keeps that device and swap chain (the window must be the same one,
   else a `Fatal`), releases the movie's objects and makes the rest as on
   Windows. **The start order on Windows is unchanged** (call 1 of wave
   B): Windows draws its movies with GDI before `Display_Setup` as before;
   the call site in `Game_WinMain` is the same on both systems, and the
   renderer comes up before the movie on Linux because the movie's first
   frame asks for it. **Which movie a Linux start plays:** `CAPCOM.AVI`
   alone (`DIV-0202 the start plays CAPCOM.AVI alone: on`), 112 frames,
   Cinepak, drawn by the renderer's device (`render: Direct3D 11 feature
   level 0xB000 on the window 640 x 480, brought up for the movies`).
2. **`DXVK_WSI_DRIVER`.** DXVK-native makes no DXGI factory while it is
   unset ("DXVK_WSI_DRIVER environment variable unset", then
   `D3D11CreateDeviceAndSwapChain` 0x80004005 - what B2b met too). The
   renderer sets it to `SDL3` before the device when the player has not set
   it (`render_d3d11.cpp` `MakeDevice`, Linux only): the game sets it, not
   the player and not the launcher, since the game alone knows its window is
   SDL3's.
3. **DXVK's logs.** DXVK writes `bof3x_d3d11.log` and `bof3x_dxgi.log` into
   the working folder, which is the game's: one run left
   `<main checkout>/bof3/bof3x_d3d11.log` (09:33; not removed by this unit,
   the folder is read only to it). The renderer now sets `DXVK_LOG_PATH` to
   the program's folder when it is unset, so the player finds them beside
   `bof3x` and `bof3x.log`.
4. **A RECT's `LONG` in the render shim** (`render_shim.cpp`). The DirectDraw
   shim's `Surface_Blt`, `Surface_BltFast`, `Surface_Lock`,
   `Viewport_Clear` and their helpers read the game's RECTs as `long`,
   which is 64 bits on Linux: every rectangle came apart, every Blt into a
   texture was refused, and every texture stayed zero - the game drew its
   lists (the frame hashes were there) onto a black picture. Now
   `Coord = std::int32_t`. Found by `BOF3X_FRAMEHASH` against the Windows
   64-bit run (the hashes differed; a texture dump showed 0 alpha, 0
   colour); after the fix every frame hash matches. `width_census.py` had
   counted these sites as `long` without telling a RECT from a `long`
   result.
5. **The disc check.** On `<main checkout>/bof3` (under `/mnt/c`, which folds
   case) the probe's local test opens `CAPCOM.AVI` and the start goes on:
   nothing else is needed. On the case-sensitive copy `~/bof3x-runs/lcB/bof3`
   the local test fails (`capcom.avi`), every drive letter answers "no
   root", and the start ends at the missing-disc message ("Please insert
   the Breath of Fire III disc.", `Game_WinMain returned 1`) - measured. So
   what the start needs on Linux is B2c's case fold for `CAPCOM.AVI`;
   nothing in the disc probe changes, on either system.
6. **The shim's surfaces above 4 GB** (met by B2c in a battle,
   `quick_combat` about frame 406: `PutObject ... lies above 4 GB`, on the
   base too). The DirectDraw shim took a surface and its pixels from the C
   runtime's `calloc`, which on Linux maps large and late blocks above 4 GB;
   the game keeps both in 32-bit words. Off Windows they now come from
   `Crt_calloc` / `Crt_free`, whose blocks are R10's arena below 4 GB
   (`render_shim.cpp` `LowAlloc` / `LowFree`); Windows keeps its `calloc`.
   The frame's own buffers (vertices, commands, the snapshot arena) stay on
   the C runtime's heap: no game word holds them. Measured after the fix,
   B2c's settings (`pc-plus-us` cache, en-US, `BOF3X_OPT=none`,
   `BOF3X_LAYERING=0`, `offscreen`): `quick_combat` to its end, recipe frame
   2920, the F6 quicksave at `Frame_Counter` 901 and its F7 restore at 1201,
   0 FATAL, no CRASH line. The `move_script::Word` fault B2c met did not
   show in this run.

Also fixed on the way: `Game_WinMain(0x0x400000, ...)` in the step-24 log
line (`%p` prints its own `0x` with glibc); an `HRESULT` printed as
`0xFFFFFFFF80004005` (a negative 32-bit value widened to a 64-bit `long`;
`render_d3d11.cpp`, `satpixie.cpp`).

**The frames, measured.** The recipe: no save, no input, `wait 1500`, with
14 shots added by `tools/recipe_shots.py --from 100 --every 100` (frames
100 to 1400 of the boot, the title and the attract's first area), point
filter, the window 640 x 480 (scale 2), `BOF3X_FRAMEHASH=1`. The Linux run
under `offscreen`; the Windows 64-bit run of the same build (the mixed copy:
the 32-bit `bof3x-launcher-dev.exe` beside the 64-bit `bof3x.exe` and
`bof3x.dll`, `--host exe`).

| Check | Result |
|---|---|
| The frame hashes (every walk of the lists, textures by content) | 1,182 ticks in both logs, **0 differ** |
| Shots f00100, f00200, f00300, f01400 | identical, pixel for pixel |
| Shots f00400 .. f01300 | differ by at most 2 in a channel: f00400 (a fade) 53,094 of 307,200 pixels, f00500 .. f01300 (the title's blended "PRESS START BUTTON") 1,980 to 2,292 pixels, all in that line. The draw lists are the same (the hashes); the difference is the two devices' blending arithmetic (llvmpipe against the Windows machine's GPU), not the game |
| The logo movie, 640 x 480 window (k = 1), `offscreen` | 98 of the 112 frames drawn (the presenter draws the frame due; shot writing slows it), **98 identical** to `bof3x-fmvdump`'s decode |
| The logo movie, 1280 x 960 window (k = 2), a WSLg X11 window (screen free that morning) | 71 drawn, **71 identical**, every 2 x 2 block the decoder's pixel |
| The logo movie, 960 x 720, `BOF3X_SNAP=0` (a fit) | drawn by the mip chain and linear sampler, no fault; 4 frames drawn of 112 (the first fit frame on llvmpipe took about 7 s); not compared - the fit is not GDI's HALFTONE |

The movie frames are taken by a developer's switch of this unit,
`BOF3X_FMV_SHOT_DIR` (Linux only): each frame drawn, the whole client as
presented, written as `AVI_<n>.bmp` before the present.

**The window icon.** `ExeIcon_Frame` ran at `Frame_Counter` 1313 under
`offscreen`: `SDL_SetWindowIcon failed: That operation is not supported`,
logged, not fatal (the offscreen driver has no icon). In the WSLg X11
window the game never reached that tick: the window never had the focus,
so `App_Active` stayed 0 and the loop waited at frame 1 (seven minutes,
then killed by its PID). That is the game's rule on both systems (the
first activation starts the loop; DIV-0033 runs on only after it), not a
Linux fault; it means a WSLg run needs the window focused once. The icon
on a real Linux window is **not verified**.

**The gate line.**

| Check | Result |
|---|---|
| Both Windows presets | build; no warning in a touched file. `codegen_same.py --by-function` against `3080284c`: 32-bit 32 symbols differ, all in `render_shim.cpp` and `render_d3d11.cpp`; 64-bit 33, those two files and 3 in `battle_menu_win.cpp` (the line numbers `Low32` takes, `__builtin_LINE`, moved by the Linux half's lines). The two render files: the RECT parameters' new mangled names (`PKl` to `PKi`, the same code), `MakeDevice` / `WindowBuffers` / `SaveTexture` split out of their callers, `g_level` |
| `os_census --check --boundary`; `linux_tree.py --check` (in the WSL, `--dxvk` DXVK's `include/native`, `--sdl` SDL3's `include`) | ok; ok, 836 files compile, 0 wait |
| `data_census --strict`, `reloc_census --check`, `reloc_bare --check`, `ledger_check`, `ledger_switches --check`, `tu_census --check --strict`, `retire_shells --check --strict` | ok |
| `width_census --check` | fails on this unit's 12 sites alone: the step-24 `%p`, the three `HRESULT` prints (`unsigned long` of the 32-bit word), four `%p` in the renderer's two new Fatals, the four COM functions whose `long` result line changed text (`Surface_Blt`, `Surface_BltFast`, `Surface_Lock`, `Viewport_Clear`) |
| The hosted shadow suite (32-bit), the exe host, the moved base | 922 `MISMATCHES` lines, every one 0, 0 FATAL, `done` (32,189 lines); exe host 10,392 lines, moved base 10,392, 0 FATAL each |
| The Linux link | no undefined name |
| `os_tests` (g++ 15), `config_tests` (WSL) | 558 checks, 0 failed; 4,400, 0 failures |
| The ELF's self-test through the launcher | 10,393 lines, 0 FATAL, `self-test only: done` |

**Not done:** the icon on a window that has the focus; the movie's fit
compared with anything; the `SDL_VIDEO_DRIVER=wayland` path (this SDL3
build has no Wayland driver: "wayland not available"); a run on the
case-sensitive copy past the disc check (B2c's).

### B2b as run (2026-11-21, branch `linux/lcB-b2b`, from `3080284c`)

Unit B2b, faults, beside B2a and B2c. Files: `src/hook/crash_posix.cpp`,
`src/hook/stackwatch_posix.cpp`, `tools/crash_report.py`,
`tools/linux_probe/crash_catch_test.{cpp,sh}`, a new
`tools/linux_probe/fault_preload.c`; and outside the unit's list, one
declaration in `src/hook/crash.h` (under `#if !defined(_WIN32)`) and step
23's lines in `src/hook/exe_host_posix.cpp` (B1's file; the change is the
step itself, below).

**How a fault ends on Linux: as on Windows, code 3, from step 23 on.**
B1's catcher ended an unhandled fault by the signal (exit 139 for
SIGSEGV), where Windows' exe host ends it with Fatal's code 3
(`exe_host.cpp` `Unhandled`, set at step 23). Decided: Linux ends as
Windows does. The reasons:

- The work order says so: `exe_host_steps.h` step 23, "an exception nobody
  handled ends the process as Fatal does, code 3", Linux column
  "crash_posix.cpp's handler's end".
- A player is told: Fatal's message ("a signal nobody handled ... see the
  CRASH lines above") where the window otherwise vanishes. *Found:* on
  this base it reaches stderr and the log only - nothing defines
  `BOF3X_OS_SDL`, which `src/os/posix/dialog.cpp` needs for SDL's message
  box, so no `Fatal` of the Linux build shows a box (measured: windowed
  runs under WSLg, Wayland and `SDL_VIDEO_DRIVER=x11` alike, ended with
  code 3 within 2 s, no box to click). The line is the build's
  (`CMakeLists.txt`, off Windows: `BOF3X_OS_SDL` on the targets that link
  SDL's video); with it, SDL on Wayland still needs `zenity` or `kdialog`
  for a box (neither is installed here).
- What reads the end: `start.cpp` waits for the game only under
  `BOF3X_SELFTEST_ONLY` and prints its code (on POSIX 128 + the signal for
  a signal's end, `os/process.h`); `tools/abc_run.py` takes any code but 0
  as a failure and prints the log's `FATAL` lines; `input_run.py` prints
  the code; `crash_report.py` reads the log and no code. None needs 139,
  and the `FATAL` line plus code 3 are the one rule both systems now give.
- The cost: the core file the signal's default would write (none by
  default here: `core_pattern` is `core`, `ulimit -c` 0). A developer who
  wants the process at the fault runs the ELF under a debugger, which
  stops at the signal before the catcher.

How (`crash_posix.cpp`): `Crash_EndUnhandledAsFatal` (declared in
`crash.h` off Windows) is step 23 in `exe_host_posix.cpp`, in place of
B1's log line. After it, a fault whose old disposition is the default or
ignored is reported as before, then the reporter thread calls `Fatal`
(its line, the state hash's flush, its message unless the run is a
self-test, `_exit(3)`); the faulting thread waits without a bound once the
report is written (the message waits for the player), and ends the
process with `_exit(3)` itself if the report is not written in 20 s. A
fault an earlier handler takes is passed to it as before; a second fault
while the first one's `Fatal` runs waits for that end; a fault on the
reporter's own thread is passed on (it would wait for itself). Before step
23 a fault ends by its signal, as one before the filter ends by its
exception on Windows. The exit trace's SIGTRAP handler ends the same way
after step 23 (Windows' filter takes an unhandled breakpoint too).

**The stack overflow.** The main thread runs on B1's placed stack
`0x60000..0x160000` with the guard page `0x5F000..0x60000` under it, and
`Crash_Start` (step 16) runs there, so the alternate stack
(`sigaltstack`, 64 KB) is that thread's. An overflow faults on the guard
page; the handler runs on the alternate stack; the report gains a line
`CRASH n: a stack overflow - the access is to the guard page ... under
thread N's stack ...` (the armed thread's stack is read by
`os::CurrentStack` at `Crash_Start`, never in the handler). Windows names
an overflow by its code `0xC00000FD`; this line is its Linux counterpart.
The stack scan finds no words in an overflow: the stack pointer itself
lies on the guard page, and the portable scan stops at the first word it
cannot read (`crash.cpp`, unchanged; a scan from the guard's top is a
change to a file Windows compiles, left).

**Where an address lies** (`crash_posix.cpp` `Where`): in the game's image
(`0x400000`, or the moved base, to the pool's end) as `the game's image, VA
0x...` - the image is anonymous memory here, no loaded object; in the
engine as `bof3x+0x...` from `image::g_ownBase`, as `crt_rest.cpp`'s
messages give it; anything else (libc, SDL, DXVK) by `dladdr`.

**The reader** (`crash_report.py --log`): a Linux report's engine is the
name its stack lines carry (`  (bof3x)`); the ELF beside the log (or
`--elf`) names those words and the fault's address by `llvm-addr2line` or
`addr2line` (the ELF is linked at a fixed address; a position-independent
one is said and left unnamed). Measured in the WSL and on Windows
(`llvm-addr2line` of the toolchain, the ELF read through
`\\wsl.localhost`): the same names.

**The stack watch** (the owner's call 2): `BOF3X_STACKWATCH=1` stays a
`Fatal` on Linux; its message now says why - a developer's switch that
measures how far Windows grows the main stack (the TEB's words, the moving
guard page, `VirtualQuery`); here the stack is mapped whole with a fixed
guard page, so nothing grows, and an overflow is the crash report's. No
ledger entry names the switch today. The line for the coordinator to
place: *`BOF3X_STACKWATCH=1` (developer): Windows only; on Linux a Fatal
that says why (the owner's call 2 of wave B, 2026-11-20), the main stack
being placed whole with a fixed guard page there.*

**Measured.**

| Check | Result |
|---|---|
| `crash_catch_test.sh` (WSL, g++ 15) | 13 modes, each end checked: `read`, `write`, `exec`, `selftest`, `overflow` by signal 11; `bus` 7; `fpe` 8; `ill` 4; `abort` 6; `trap` 5; `unhandled`, `overflow_unhandled`, `trap_unhandled` exit 3 with one `FATAL: a signal nobody handled`; both overflows with the guard page's line. The test's doubles were stale (a PEB at `gs:0x60` for `ImageBase()`): they are the start-up's variables now |
| The ELF, through the launcher, before step 23 (`BOF3X_EXEFILE=1 BOF3X_SELFTEST_ONLY=1 BOF3X_CRASH_TEST=0`, offscreen, the PC install) | `CRASH 0: signal 11 (SIGSEGV, code 1) at 0x...100701F6 (bof3x+0x701F6)`, the crash test's thread; the catcher armed on thread 1010's stack `0x60000..0x160000`; the launcher: `game process exit code 139`. `crash_report.py`: the fault named `bof3::(anonymous namespace)::SelfTest(unsigned long) (crash_posix.cpp:296)` |
| The ELF past step 23 | `BOF3X_CRASH_TEST=<seconds>` cannot reach it on this base: the game is past step 23 for well under a second before the renderer stops it (`FATAL: render: D3D11CreateDeviceAndSwapChain failed, HRESULT 0x80004005`, offscreen and in a WSLg window alike, B2a's files), and the switch counts whole seconds from step 16. So `fault_preload.c`: an `LD_PRELOAD` library of the test's own, standing in for `SDL_CreateWindow`, which `Game_WinMain` calls first on the main thread, faults there (the game unchanged, no window made) |
| - a read of `0x10` there (offscreen, launcher, an install of links to the PC install without its movies) | the report on the main thread, `rsp 0x15FB10` on the placed stack; `FATAL: a signal nobody handled: 11 (SIGSEGV, code 1) ...`; the same game started directly with the environment the launcher gave it: **exit 3**. `crash_report.py`: every stack word named, `game_window::Create(bool) (game_window.cpp:413)`, `Game_WinMain (magic_080.cpp:704)`, ... `EngineMain(void*) (exe_host_posix.cpp:764)`, `os::(anonymous namespace)::Entry() (placed.cpp:42)` |
| - a recursion with no end there | `writing 0x5FE50`, `rsp 0x5FE50`; `CRASH 0: a stack overflow - the access is to the guard page 0x5F000..0x60000 under thread 4530's stack 0x60000..0x160000`; the `FATAL` line; direct: **exit 3**. The handler ran on the alternate stack with the main stack spent |
| - the same in a WSLg window (09:56, the screen free), Wayland and `SDL_VIDEO_DRIVER=x11` | the report and the `FATAL` line, then the end within 2 s with no box: see the finding above |
| `BOF3X_STACKWATCH=1` on the ELF (self-test, launcher) | `FATAL: BOF3X_STACKWATCH=1 is refused on Linux (the owner's call 2 of wave B, 2026-11-20): ...`; the launcher: `game process exit code 3` |
| WSL gates | the Linux preset links, no undefined name, no warning in a file touched; `os_tests` (the brief's g++ line) 558 checks, 0 failed; `config_tests` 4,400 checks, 0 failures; the ELF's self-test through the launcher (`BOF3X_EXEFILE=1`): 10,393 lines, 0 FATAL, `self-test only: done` |
| Windows: both presets | build, no warning; the one file a Windows build compiles that this unit changed is `crash.h` (comments and a declaration under `#if !defined(_WIN32)`); `codegen_same.py --by-function`, the objects with `3080284c`'s `crash.h` against this branch's: i686 29,430 function definitions, x86_64 20,896, **0 differ** each. So the hosted suite was not run (no Windows object differs) |
| The censuses | `os_census.py`, then `--check --boundary`: ok (8 files wait for 1 later unit); `linux_tree.py --check` (in the WSL, `--sdl` and `--dxvk` at `~/src/b1deps`): 836 files compile, 0 wait; `data_census --strict`, `reloc_census --check`, `reloc_bare --check`, `ledger_check` (242 entries, 0 errors), `ledger_switches --check`, `tu_census --check --strict`, `retire_shells --check --strict`: ok. `width_census --check`: 27 new sites, every one in `crash_posix.cpp` (the new `uintptr_t` and `long` of the armed stack's range and its log lines) - expected for new sites, not regenerated here |

**The install for the runs past step 23.** The launcher's game folder must
not be the owner's install for a run that writes, and the movies stop the
game on this base (B2a's `Draw`): `~/src/b2b-game` holds links to
`<main checkout>/bof3`'s folders and large files, copies of its small ones
(the saves, `BOF3.CFG`), no movies, and an empty `CAPCOM.AVI` of ours - the
disc probe (`magic_080.cpp`, DIV-0100) asks for the name, and a movie that
does not open is skipped as the original skips it. On ext4 the probe's
names need their case (`CAPCOM.AVI`, `BOF3.EXE`): B2c's fold is not on this
base; `/mnt/c` folds case itself, which is why runs on `<main checkout>/bof3`
never showed it. DXVK writes `bof3x_d3d11.log` into the game folder unless
`DXVK_LOG_LEVEL=none` (B2a's runs on `<main checkout>/bof3` leave it there).

**For the coordinator's gate:** with B2a's renderer up, the switch itself
past step 23 is `BOF3X_EXEFILE=1 BOF3X_CRASH_TEST=10 ./bof3x-launcher --game
<install> --no-config` (a window, or offscreen if B2a's surface allows);
pass: the `CRASH 0` lines, `FATAL: a signal nobody handled: 11 (SIGSEGV,
code 1)`, and `tools/crash_report.py --log build-linux-x86_64/bof3x.log`
naming the fault `SelfTest`.

### B2c as run (2026-11-21, branch `linux/lcB-b2c`, from `3080284c`)

Unit B2c: paths and saves, beside B2a and B2b. Every game run below used
the PC install on the WSL's own disk (ext4, which looks at case):
`~/bof3x-runs/lcB/bof3`, read only, or a copy of it of B2c's where a run
writes. `/mnt/c` folds case by itself, so nothing there proves a fold.

**The fold** (the owner's call 3 of 2026-11-19). `src/os/case_fold.h`, its
POSIX half `src/os/posix/case_fold.cpp`, and a Windows half that gives the
name back as asked (Windows' filesystems fold already). `os::Path` on POSIX
calls it for every narrow name it converts, after the case check has looked
at the name as asked; so the game's `Crt_fopen`, the `ifstream`s on
`os::Path`, `IsFile`, `WalkDirectory` and `FindFirst` all take it. The rule:

- Only a relative name (the game folder is the working folder, which
  `bof3x-launcher` starts the game in) with no `..` part. An absolute name -
  the cache, the home, a `BOF3X_` path, the temporary folder - is never
  folded.
- The exact name first (`lstat`): when it is there it is the name, whatever
  else the folder holds. Otherwise one walk per part from the game folder
  down: a part the folder holds exactly is kept, else the entry equal to it
  but for ASCII case; a part with none, or with `*` or `?` (a walk's
  pattern, which the walk matches without case itself), ends the fold and it
  and the rest stay as asked, so a file made in a folder of another case
  lands in that folder.
- **Two names that differ in case alone** (`OYASUMI.DAT` and `Oyasumi.DAT`
  in one folder, which no Windows folder can hold): the exact one when asked
  exactly, else the first in byte order - which is the entry `WalkDirectory`
  hands first (upper-cased order, ties by the bytes), so an open and a walk
  take the same file on every run whatever order the filesystem lists in. In
  ASCII the upper case comes first: `OYASUMI.DAT`.
- Each folder's listing is cached and walked again when the folder's device,
  inode, change or modify time is not what it was (a quicksave renamed into
  it). Measured: the 1,261 names of the install that are not in upper case,
  asked in upper case, open, with 4 walks (the game folder, `SND`, `BGM`,
  `DAT`).
- Under `BOF3X_CASECHECK=1` a folded name gives one line, once:
  `CASEFOLD asked <name> opened <name as the disk holds it>`, beside the
  check's own `CASECHECK asked ... found ...`. With the switch off the fold
  says nothing (the case check's channel reaches the log only under the
  switch, `hook/log.cpp:141`) and costs one `lstat` of a relative name.

**The failure before the fold and the pass after** (a full start of the ELF
through the Linux launcher, `BOF3X_EXEFILE=1`, `SDL_VIDEO_DRIVER=offscreen`,
`BOF3X_CASECHECK=1`, a copy of the PC install on ext4):

| Build | What the log says | Where it stops |
|---|---|---|
| `3080284c` | `CASECHECK asked CAPCOM.AVI found capcom.avi`, then `Game_WinMain returned 1` | The first name that fails is `CAPCOM.AVI`, the disc probe's local test (`Disc_Probe`, `file.cpp:40`): no local movie, no CD-ROM drive (`GetDriveTypeA`'s slot answers "no root"), so "Please insert the Breath of Fire III disc." and exit code 1 |
| the branch | `CASECHECK asked CAPCOM.AVI found capcom.avi`, `CASEFOLD asked CAPCOM.AVI opened capcom.avi`; the window (offscreen), `DIV-0035 CAPCOM.AVI into the window`, Cinepak 640 x 480, 112 frames | The probe passes; the movie's first frame stops at `battle_menu_win.cpp:1069`'s Fatal (the movie's picture off Windows: B2a's) |

The names of wave A's list, asked in Capcom's case through the branch's
seam on the coordinator's copy: `CAPCOM.AVI` to `capcom.avi`, `LOGOS.AVI`
to `logos.avi`, `SND\OYASUMI.DAT` to `SND/Oyasumi.DAT`, `BGM\000.DAT` to
`BGM/000.dat`, and the `SND` streams `004_00`, `Akuryou`, `Dragon`,
`Gaan01`, `Item01`, `Mimi01`, `Nakama02`, `Sippai01`, `Win` - each opens.

**Past the movie.** With the movie unreadable (a test copy whose
`capcom.avi` is a folder: the probe opens it, the movie's read refuses, and
`Fmv_Play` skips it as for a missing file) and `DXVK_WSI_DRIVER=SDL3`, the
ELF reaches `Display_Setup` (DXVK 3.1.1 on llvmpipe, a swap chain from SDL's
offscreen driver: five images, mailbox), the loop and the title (`FIRST.DAT`,
`DEMO.DAT`, the title's music) and runs until ended. Without
`DXVK_WSI_DRIVER` DXVK makes no factory (`D3D11CreateDevice: Failed to
create a DXGI factory`): B2a's. `quick_combat.txt` (combat's first 900
frames, F6 at 900, F7 at 1200) from the `combat` save in slot 0, en-US from
`analysis/cache/pc-plus-us`: the save loads (`save loaded slot 0
(BISLPS00.DAT)`), the battle starts, and the run stops before frame 900,
so no quicksave was made on Linux:

| Run | Ends |
|---|---|
| the branch, 1 | frame 471: `CRASH 0: signal 11` at `move_script::Word` (`move_script_bytes.h:27`), reading through `r11 = 0x00402FD8_0093BA04` - two 32-bit words read as one 64-bit pointer; area 0x1D, mode 5 |
| the branch, 2 | frame 406: `FATAL: render_shim.cpp:534 PutObject: 0x7ed28c06df30 lies above 4 GB` |
| `3080284c` (a copy with `CAPCOM.AVI` a folder in upper case, so the base's probe passes) | the same `PutObject` Fatal |

Both are outside this unit's files (the renderer's objects above 4 GB;
a pointer of two halves in a battle's move script, which a 64-bit Windows
run does not meet). The start also takes about two minutes from the
launcher to the game's loop on this machine.

**Where a Linux run puts things** (the files written, listed after the
runs): the game's saves `BISLPS0x.DAT` and the quicksaves `BISLPS1x.DAT` in
the game folder, the working folder, as on Windows; `bof3x.log` and
`bof3x.window` beside the ELF (`LogOpen`: the module's path with `.log`);
DXVK's own `bof3x_d3d11.log` and `bof3x_dxgi.log` in the game folder - the
working folder, DXVK's default when `DXVK_LOG_PATH` is unset - **which a
read-only install refuses and a player's install collects**: the
launcher's or B2a's to set (`DXVK_LOG_PATH`, or `DXVK_LOG_LEVEL=none`).
`bof3x.ini` is the launcher's home (its own folder, or `BOF3X_HOME`); with
`--no-config` nothing is written there. The coordinator's copy was not
written by any run.

**The backslash in the save list's lines.** `/tmp/bof3x_qm_a_N\BISLPS??.DAT`
was built by `quick_menu.cpp`'s `ListsSelfTest` (`"%sbof3x_qm_a_%u\\"`),
then read through the seam, which takes a backslash for a separator
(`os::Path`); so no file of that name was ever made (`find /tmp
~/bof3x-runs/lcB -name '*\*'`: none, before the change and after). The two
folders now end in the system's separator, as the temporary folder's name
does: the same bytes on Windows, a slash on Linux, and the self-test's
seven `save list   /tmp/bof3x_qm_a_N/BISLPS??.DAT` lines read so. The one
backslash left in a Linux self-test's log is `DIV-0086: optional layer 1,
DAT\area4-walls.*.DAT`, a relative pattern the engine builds in Capcom's
form (`"DAT\\%s"`) and passes to the walk, which reads it through the seam:
not a file's name.

**D431's open item.** Read again, the gap is not the one D431 records: the
next F6 does not refuse. After a crash between the third way's claim and
its rename at step `k -> k+1`, the slots above `k+1` have each moved up, `k`
is where it was and `k+1` is the empty claim; the next rotation moves the
empty file up as a slot (`k+1 -> k+2`, its target vacated by the step
above), then `k -> k+1`, and F6 succeeds - with a file of zero bytes in the
history, which the quick list shows empty and F7 refuses ("not a save
file"). The fix keeps the seam's rule ("an empty file at the target is a
file: refused too") and is made where a slot's meaning is known:
`snapshot.cpp`'s `Rotate` removes a slot file of zero bytes when its step
comes, as a gap, with a log line (`quicksave   BISLPS15.DAT was empty ...:
removed, a gap`). No build writes one: `WriteNewest` writes the temporary
file whole and renames it. A new self-test case builds the crash's state
(10..14 in place, an empty 15, 16..1F one up, the crashed take in
`BISLPS10.TMP`) and checks that the next F6 leaves every quicksave one up,
16 a gap, 10 the new one and no file of zero bytes. On Windows the same
code runs; only a zero-byte `BISLPS1x.DAT` a person put there is now
removed by F6 where it was moved up the history.

**The unattended marker.** `os::CloseNamedEvent` (`os/named_event.h`, both
backends: `CloseHandle`; `sem_close` and `sem_unlink`) gives a named
event's name back, with a test (the name gone from `/dev/shm`, made again).
Nothing calls it yet: `exe_host_posix.cpp` (step 17) keeps the marker's
pointer nowhere, and a run ends by `_Exit`, `Fatal` or a signal. Each Linux
run so far leaves `/dev/shm/sem.bof3x_unattended_<pid>` (eight after this
unit's runs); the next process with that number unlinks it before it makes
its own, so they do not collide, but they pile up until a reboot.

**Measured.**

| Check | Result |
|---|---|
| `os_tests`, the fold's block (`TestCaseFold`) and the close (`TestNamedEventClose`) | WSL, g++ 15 by the brief's line and the CMake build: 596 checks, 0 failed (8 walks; each name in each case, a missing name, a folder of another case, two names that differ in case alone, a walk's pattern, an absolute name, `..`, the cache walked again after a file was made, `errno` kept, the line once); Windows i686 643 to 644 and x86_64 647, 0 failed (the fold gives the name back; the system folds) - one i686 run failed the auto-reset event's two-waiter check (`os_tests.cpp:1114`, not this unit's) while a build ran beside it, and passed twice after |
| `config_tests` (WSL) | 4,400 checks, 0 failures (`save_tests` fails to compile under g++ as at the base) |
| The Linux link | no undefined name |
| The ELF's self-test through the Linux launcher | `/mnt/c` install (`BOF3X_EXEFILE=1`): 10,394 lines, 0 FATAL, `done` (one more than B1's: the empty slot file's line); the ext4 copy with `BOF3X_CASECHECK=1`: 10,395, the same, no `CASECHECK` line |
| `linux_tree.py --check` (WSL) | 837 files, 837 compile, 0 do not, 11 Windows-only |
| `codegen_same.py --by-function` against `3080284c` | the engine: 32-bit 18 symbols, 64-bit 5, every one in `snapshot.cpp` or `quick_menu.cpp` (the 32-bit rest are their exception tables renumbered); the seam library (`build*/src/os`, which the tool's default root does not read): the five new functions and nothing changed (32-bit: `file.cpp`'s exception tables renumbered) |
| `os_census.py --check` | 1,898 sites: one new, `CloseHandle` in `win32/named_event.cpp`; `--boundary` ok |
| The other censuses | `data_census --strict`, `reloc_census`, `reloc_bare`, `width_census`, `ledger_check`, `ledger_switches`, `tu_census --strict`, `retire_shells --strict`: ok |
| The hosted shadow suite (32-bit), the exe host, the moved base (Windows, under the lock) | 922 `MISMATCHES` lines, every count 0, 0 FATAL, `done` (32,190 lines); the exe host and the moved base (`BOF3X_EXEFILE=1`): 10,393 lines each, 0 FATAL, `done` - one more than B1's, the empty slot file's line, which the hosted suite shows too |

**Not run:** the quicksave made and loaded on Linux in a game (the route
stops before F6, above); the `combat` route's first minute to its end and
on a disc-built install; a Windows run with `BOF3X_CASECHECK=1` (wave A's).

### B2d as run (2026-11-21, branch `linux/lcB-b2d`, from `360260c4`)

Unit B2d, alone after B2a..B2c: the routes on Linux - the harness, and what
the ten state-hash routes find. Every game run below: the ELF through
`bof3x-launcher` beside it, headless, under the round's lock, on a quiet
machine (no compiler seen in the WSL or on Windows during any hashed run).

**The harness** (`tools/input_run.py --host linux`, `tools/linux_run.py`).

- **Through the launcher**, by default: `bof3x-launcher --game G
  --no-config` beside the ELF (or `--launcher`), as the Windows runs'
  launcher. It returns once the game is up; the game is followed by the pid
  of its `started ... (pid N)` line (`/proc/<pid>/stat`, not a zombie) and
  ended by that pid alone. `--direct` keeps C8's start of the ELF itself.
  From Windows the same runs through `wsl -e` (the launcher's folder as the
  working folder; the pid asked of the WSL helper's new `alive` operation).
- **A game folder made for the run** (`--run-game DIR`, default `OUT/game` on
  Linux; `linux_run.make_run_game`, also the helper's `mkgame`): **links**
  into `--game` for every folder (`BGM`, `DAT`, `SND`, `CDImage`,
  `directx7`) and every file the game only reads (`BOF3.exe`, the movies,
  `DATA.TAG`, the installer's files); **real copies** of what a run writes:
  the slots `BISLPS0?.DAT` and `BOF3.CFG` (the launcher writes its display
  lines); the recipe save (`# save NAME`, `tools/recipe_saves/NAME.DAT`,
  found in this checkout, beside the game, or the main checkout's from a
  worktree) as slot 0. Not taken: quicksaves, `.recipe_slot0`, `*.TMP`,
  DXVK's logs. The source (`~/bof3x-runs/lcB/bof3`) is never written; a
  folder is removed again only when it carries the marker the function
  writes. The other slots stay `--game`'s, because the Windows references
  had the owner's slots beside the swapped slot 0, and a route that opens
  the load menu reads every slot's header into `.data` (the slots of
  `<main checkout>/bof3` and of the copy: the same bytes, unchanged since
  2026-10-28). The slot-0 swap and its lock are not needed: the folder is
  the run's. With the save in place for the whole run, every route is as
  `--slot0-hold` (C0 held it on `shop` alone; the other nine never read
  slot 0 after its load).
- **Headless**, by default (`--window` keeps the caller's display):
  `SDL_VIDEO_DRIVER=offscreen` and `SDL_AUDIO_DRIVER=dummy` unless the
  caller set them, and no `DISPLAY` or `WAYLAND_DISPLAY` in the game's
  environment (through the WSL: `env -u`). SDL3's dummy audio device takes
  the stream a buffer at a time at the device's rate and plays it nowhere:
  the mixer, the cursors, the ring's notifications and `Music_Pump` run as
  with a device. `music_heard.py` on Linux runs with `BOF3X_MUSIC_LOG=1`:
  `whelpBoss` pass, 164 ring lines, 6 tracks; `cutsceneAndNue` pass, 116
  lines, 5 tracks - the counts of C0's 64-bit Windows runs.
- **`BOF3X_EXEFILE=1`** by the Windows runners' rule (a PC folder holding
  `BOF3.exe`, no cache named): the references read `.data` from the file.
- **The settings of the references, and what is not the same.** C0's
  environment (`BOF3X_OPT=none`, `BOF3X_LAYERING=0`, the skip list, no other
  `BOF3X_` variable, no `BOF3X_LANG`: the launcher's language with no ini,
  `original`), `input_run.py`'s additions (New Game's two screens off,
  `BOF3X_SHOT_WAIT`, `BOF3X_SHOT_DIR`, `BOF3X_PRESENT=clean`, the recipe),
  the launcher's defaults with no ini (`preset recommended, language
  original` in both logs; windowed 640 x 480, scale 2, linear present, as
  C0's log). Not the same, by the host: the desktop (Windows 3440 x 1440,
  SDL's offscreen driver a fixed 1024 x 768), the sound device (Windows' 441
  frames a buffer, SDL's dummy 1024), the GPU (llvmpipe), the install's
  file system (ext4, the case fold), the slot 0 swap (none here).
- **A Fatal's message box cannot hold a run.** `BOF3X_CRASH_TEST=60`,
  headless, through the launcher, the run folder of `combat`: the game's
  environment has neither display variable (read from
  `/proc/<pid>/environ`); `CRASH 0: signal 11`, `FATAL: a signal nobody
  handled`, and the process gone 60.1 s after the launcher returned, with
  nothing killing it. Three routes that met a fault on the first pass
  (below) ended the same way, the harness reporting `end: FATAL`.
- Windows' path through `input_run.py` is the same: the 64-bit exe host of
  this branch's build, C0's conditions, `shop` with `--slot0-hold` -
  **identical to `cn_shop_c0x64` on all 3,137 ticks**, and the owner's slot 0
  back, byte for byte.

**The first finding: a Linux hash file compares with a Windows one as
built.** The header's image place (`0x400000`), its fold (none), the extent,
the skip list's hash (`2C491AB2`, 173 ranges) and the flags (our pointers by
what they name, the battle message words, bound code words as their VAs)
are the references'. Our image is an ELF at `0x10000000`: the census reads
its `PT_LOAD` segments through `os::ModuleImage`, and the bound code places
are `bind.cpp`'s at `0x7C400000..` on both systems. Two Linux runs of a
route are identical on every tick (seven routes run twice; `whelpBoss` with
and without `BOF3X_MUSIC_LOG`).

**What the routes found, and fixed** (each a defect of the Linux build;
the same code on Windows unless said):

1. **`Window_DispatchKind`'s handlers** (`menu_list.cpp`; the
   `move_script::Word` fault B2c met). The dispatcher calls the three gauge
   handlers through a type of eight `std::uint32_t`; the handlers take eight
   pointers. At 64 bits the 7th and 8th arguments lie on the stack (the 5th
   to the 8th on Win64): a 32-bit word stored into an 8-byte slot leaves its
   high half as it was, and the handler read `step` as
   `0x00402FD8'0093BA04` (B2c's `r11`; `movsx esi, word [r11]` at
   `Window_HpGaugeTrack`, by `addr2line` on B2c's ELF). It depends on what
   the slot held before, which is why B2a's run of the same route did not
   meet it. Now the words go out as `std::uintptr_t`, zero-extended. Win64
   had the same latent read (it passed because the stale high halves there
   were zero); its code changed (below), its behaviour not.
2. **`Crt_sprintf` and `Crt_sscanf` through `__cdecl` types**
   (`map_field_objects_callees.h`, `shell_callees.h`, their fills in
   `area_map.cpp` and `magic_080.cpp`). On Linux `__cdecl` is `ms_abi` and
   the two variadic functions are System V (LB0: no convention for a
   `...`): three types still said `__cdecl`, so the caller put (buffer,
   format, value) in `rcx`, `rdx`, `r8` and the callee read `rdi`, `rsi`,
   `rdx`. `field_menu`, `menu_screens` and `worldMapAndAreaTransition`
   faulted in `Crt_sprintf` reading `0x41` (the member status's level).
   A census of the class (a script of this unit's, over every typed function
   pointer whose comment names its target in `symbols.gen.h`: 897 types)
   found these three and no other; types filled by an unnamed cast are not
   covered by it.
3. **The DirectSound layer's LONGs** (`sound_sdl.cpp`, `wav_dump.cpp`):
   below, the width list.

**The ten routes on the final build** (`dedc9245`'s ELF, tag `t3`, against
`<main checkout>/analysis/statehash/cn_<route>_c0x64.sh`):

| Route | End | Seconds | `diff` against `_c0x64` |
|---|---|---:|---|
| `field_menu` | recipe done, frame 1454 | 57 | the three desktop pages |
| `menu_screens` | done, 1770 | 67 | the three desktop pages |
| `combat` | done, 2620 | 95 | the three desktop pages |
| `shop` | done, 3157 | 114 | the three desktop pages; 8 pages at ticks 2713..2756 |
| `dragonTransform` | done, 4329 | 152 | the three desktop pages |
| `cutsceneAndNue` | done, 7421 | 256 | the three desktop pages |
| `whelpBoss` | done, 13181 | 448 | the three desktop pages |
| `worldMapAndAreaTransition` | done, 2143 | 80 | the three desktop pages; `0x6C3000` from 1098 |
| `caughFish` | done, 3889 | 138 | the three desktop pages; `0x6C3000` from 395 |
| `masterAndManillo` | done, 5519 | 192 | the three desktop pages; `0x6C3000` from 324 |

Each run: 0 FATAL, no `CRASH` line, within its recipe's time (the seconds
are the launcher to the recipe's end; the start to the game's loop takes
about 45 s here, not B2c's two minutes, the game folder on ext4).

- **The three desktop pages** (`0x6C9000`, `0x7C9000`, `0x939000`, every
  tick): raw dumps of `shop` at tick 2712 on both systems (Windows: the
  64-bit run above with `BOF3X_STATEHASH_DUMP`) differ outside the skip list
  in three ranges alone - `0x6C9F40` and `0x7C9F44` (the desktop's
  `VERTRES` and `HORZRES` of `Display_Setup`, `display-setup.md` 2.1) and
  `0x939A30..0x939A35` (`Desktop_Height`, `Desktop_Width`): 1440 / 3440 on
  the Windows machine, 768 / 1024 from SDL's offscreen driver. A property of
  the host's screen: two Windows machines with different desktops differ
  there too.
- **`0x6C3000`** (Direct3D's state): the Windows references disagree with
  each other on it on these three routes; `check` with `_c0x32` as the
  second reference counts it as noise and reports the three desktop pages
  alone. Two Linux runs agree on it.
- **`shop`'s window 2713..2756** (`0x66C000`, `0x7E0000`, `0x903000`,
  `0x929000`, `0x7DE000`, `0x7E1000`, `0x803000`, `0x7CA000`; the pages
  `state-hash.md` section 6 names for DIV-0201, "the mode transition after
  the inn's jingle comes a frame apart"): the dumps at 2713 show Linux's
  transition begun (`Transition_Busy` and `ModeFlow_TransitionKind` 1, the
  ordering-table heads moved) where Windows' has not; at 2720 the two are
  equal again but for the desktop; at 2740 the message box is a step apart
  (`Msgbox_StepCount` 3 / 4, the pen 0x52 / 0x5E); from 2757 equal to the
  end. The jingle (`SND\OYASUMI.DAT`) opens at frame 2513 on both; its end
  is read from the sound device's cursor, which SDL's stream advances a
  device buffer at a time. With the dummy device at 441 frames a buffer
  (`SDL_AUDIO_DEVICE_SAMPLE_FRAMES=441`, Windows' size) the window starts a
  tick later (2714..2759): the device's clock, not the game.

**For the coordinator (nothing of this changed here):** the desktop words
`0x6C9F40 4`, `0x7C9F44 4`, `0x939A30 8` are the host's and no system's
defect; if the hash should not see them, that is a skip-list change and new
Windows references. Short of that, the gate reads them as this table does.
The `shop` window is the sound device's clock (DIV-0201's class) and cannot
be skipped (game state follows from it); it is the same on every Linux run.

**The width sites** (task 2's search: `width_census.py --list long`, 2,349
rows, 1,789 of them `symbols.toml` ctypes that `gen_symbols.py` already
writes as fixed-width; the source rows read one by one, and a scan of every
function of 7 or more parameters with an 8-byte one on the stack):

| Site | What | Done |
|---|---|---|
| `sound_sdl.cpp` `BufGetVolume`, `BufGetPan` (`long*`) | the game's out-local is `std::int32_t` (`sound_sp.cpp:118`): 8 bytes written into 4 | `std::int32_t*` |
| `sound_sdl.cpp` `BufSetVolume`, `BufSetPan` (`long`) | the caller's `std::int32_t` arrives zero-extended: -600 read as 4,294,966,696 and refused, every level and pan lost | `std::int32_t` |
| `sound_sdl.cpp` `Position` (`unsigned long offset`) | `music.cpp:503`'s `std::uint32_t` offset and its padding read as one 8-byte number: a notify offset past the buffer, refused | `std::uint32_t` |
| `sound_sdl.cpp` `Buffer::volume/pan`, `Gain`, `kVolumeMin..`, the self-test's `MGetLong`, `MLong` | the model of the two above | `std::int32_t` |
| `wav_dump.cpp` `HookSetVolume`, `HookSetPan`, `Buffer::volume/pan`, `Gain` | the same LONGs through the dump's hooks | `std::int32_t` |
| `menu_list.cpp` `Window_DispatchKind` | eight words to eight pointers, two on the stack | `std::uintptr_t` |
| HRESULT results as `long` (`sound_sdl.cpp`, `render_shim.cpp`, `wav_dump.cpp`, `bind.cpp` `DirectSoundThunk`, `exe_host_posix.cpp` `SlotDirectSoundCreate`) and `AddRef` / `Release` as `unsigned long` | the caller reads the low 32 bits of the register: the same value | left |
| `render_shim.cpp` RECTs | B2a's `Coord` | (B2a) |
| `wav_dump.cpp` `Buffer::bytes/frequency/lock_offset` (`unsigned long`) | the dump's own model, filled from `std::uint32_t` | left |
| `input_script.cpp` `InputScript_RecordKey(unsigned, long)`; `statehash.cpp` `StateHash_SetImageBase(unsigned long)`; `strto32.h`'s `strtol` locals; `crash*.cpp`, `exe_host_posix.cpp`, `stallwatch.cpp` print casts | called directly with the declared type, or a print | left |
| `stackwatch.h` `StackWatch_AtSetStackBase(unsigned long)`; `gapwatch.cpp`, `write_watch.cpp`, `calltrace.cpp` | Windows only (`linux_tree.py`'s `WINDOWS_ONLY`, the stack watch refused on Linux) | left |
| `gte.cpp` `Gte_RotTransPers3/4`, `Gte_RotAverage3/4`; `sound_sdl.cpp` `BufLock` (pointers on the stack) | every caller's type spells the same pointers | left |

**Windows** (`codegen_same.py --by-function` against `linux/lcB`'s builds of
`360260c4`): 32-bit, 15 symbols - the eight renamed methods (`long` to
`int` in the mangled name, the bodies the same hash) and the seven that name
them; 64-bit, 26 - those, `Window_DispatchKind` (the zero-extended words),
and the line numbers `Low32` takes (`__builtin_LINE`), moved by this unit's
comments in `menu_list.cpp`, `sound_sdl.cpp` and `wav_dump.cpp`. The callee
types and the two casts changed no code in either preset.

**The gate line.** Both Windows presets build (no warning in a line this unit
touched); the censuses pass but `width_census --check`, which names this
unit's 12 changed lines (`sound_sdl.cpp` 6, `wav_dump.cpp` 4,
`menu_list.cpp` 2) and is not regenerated here; `linux_tree.py --check` (in
the WSL): 837 files compile, 0 wait. In the WSL: the Linux preset links with
no undefined name; `os_tests` 596 checks, 0 failed; `config_tests` 4,400, 0
failures; the ELF's self-test through the launcher (`BOF3X_EXEFILE=1`,
offscreen): 10,394 lines, 0 FATAL, `self-test only: done`. On Windows,
under the lock: the hosted shadow suite (32-bit) 922 `MISMATCHES` lines,
every one 0, 0 FATAL, `done` (32,190 lines); the exe host (`BOF3X_EXEFILE=1`)
10,393 lines, 0 FATAL, `done`.

**Not done:** the routes on a disc-built install; a window (WSLg) run; the
`shop` window bisected below the sound device's clock (one hypothesis
tested, the buffer size); the general census of calling-convention
mismatches for pointer types filled by unnamed casts.

### Wave B as run (2026-11-21, branch `linux/lcB`, from `main` `ffb2c892`; not pushed)

**The units.** B1, then B2a, B2b and B2c side by side, as staged; then a
fifth unit the staging did not have, **B2d**, because the first routes on the
ELF faulted and no unit owned the routes' harness or the width defects under
them. Each was a worker's branch in its own worktree, merged `--no-ff` one
at a time, built for Linux and for both Windows presets after the merge, and
its proof run again there. Their sections are above. The coordinator's own
commits: the censuses after B1, after the B2 units and after B2d; `Fatal`'s
message box on Linux (`BOF3X_OS_SDL` was not defined for the seam, B2b's
finding); the unattended marker's name given back at each orderly end (no
`bof3x_unattended` name is left under `/dev/shm`); the ledger's and the
defect list's lines below.

**The gate (B3), on the merged branch** (the ELF of `3fa64b51`'s tree, md5
`a27f931a...`; WSL Ubuntu, llvmpipe, SDL's `offscreen` driver and `dummy`
audio device, no `DISPLAY` and no `WAYLAND_DISPLAY`; through
`bof3x-launcher`; the game folder a copy of the PC install on ext4, each run
in a folder of links made by `tools/input_run.py --host linux`; C0's
settings, `BOF3X_OPT=none BOF3X_LAYERING=0` and the skip list). Scripts:
`C:/Users/kerok/bof3x-runs/lcB/coord/` (`route.sh`, `b3_all.sh`,
`b3_rest.sh`); hash files and logs: `~/bof3x-runs/lcB/b3/out/` in the WSL.

| Route | Seconds | Ticks compared | `diff` against `_c0x64` | `check` with `_c0x32` as second reference |
|---|---:|---:|---|---|
| `field_menu` | 57 | 1,409 | the 3 desktop pages | the same 3 |
| `menu_screens` | 67 | 1,729 | the 3 desktop pages | the same 3 |
| `combat` | 95 | 2,561 | the 3 desktop pages | the same 3 |
| `shop` | 113 | 3,137 | the 3 desktop pages, and 8 pages within ticks 2713..2756 | the same 11 |
| `dragonTransform` | 153 | 4,289 | the 3 desktop pages | the same 3 |
| `cutsceneAndNue` | 256 | 7,396 | the 3 desktop pages | the same 3 |
| `whelpBoss` | 448 | 13,163 | the 3 desktop pages | the same 3 |
| `worldMapAndAreaTransition` | 80 | 2,113 | the 3 desktop pages, and `0x6C3000` from tick 1098 | the 3 desktop pages |
| `caughFish` | 137 | 3,841 | the 3 desktop pages, and `0x6C3000` from tick 395 | the 3 desktop pages |
| `masterAndManillo` | 192 | 5,505 | the 3 desktop pages, and `0x6C3000` | the 3 desktop pages |

Every run: the recipe's end reached, 0 `FATAL`, no `CRASH` line, no compiler
seen beside it. This is B2d's table again, page for page, on the merged
build. **How the differing pages are read** (B2d measured each, "B2d as
run"):
- **The 3 desktop pages** (`0x6C9000`, `0x7C9000`, `0x939000`, every tick):
  the desktop's size, 3440 x 1440 on the Windows machine and 1024 x 768 on
  SDL's offscreen driver; no other byte outside the skip list differs in
  B2d's raw dumps. The host's, not a defect. The hash still sees them: to
  hide them is a skip-list change and new Windows references, which was not
  done.
- **`0x6C3000`** on three routes: the two Windows references disagree there
  themselves (the staging's allowance); `check` drops it.
- **`shop`, ticks 2713..2756**: the mode transition after the inn's jingle
  starts a tick apart, and the two runs are equal again from 2757 to the
  end. The jingle's end is read through the sound device's cursor
  (DIV-0201's class); with the dummy device at Windows' buffer size the
  window moves by a tick. The same on every Linux run. **This is a
  difference the gate does not explain away**: game state follows from it
  for 44 ticks, and it is accepted here as the device's clock, on B2d's one
  test.

| Check | Result |
|---|---|
| `music_heard.py`, `whelpBoss` | pass: 164 ring lines, 6 tracks, heard advanced on every playing stretch (C0's counts) |
| `music_heard.py`, `cutsceneAndNue` | pass: 116 ring lines, 5 tracks |
| `quick_combat` (F6 at frame 900, F7 at 1200) with `BOF3X_CASECHECK=1` | the route's end reached, 0 `FATAL`; `F6 at Frame_Counter 901: BISLPS10.DAT, 4786341 bytes`, `F7 BISLPS10.DAT restored at Frame_Counter 1201: back to Frame_Counter 901`; the one fold of the run: `CASECHECK asked CAPCOM.AVI found capcom.avi` |
| A fault past step 23 (`BOF3X_EXEFILE=1 BOF3X_CRASH_TEST=60`, headless) | `CRASH 0: signal 11 (SIGSEGV, code 1)`, `FATAL: a signal nobody handled: 11`, the game gone by itself 60 s after the launcher returned; `crash_report.py` names `SelfTest (crash_posix.cpp:296)`. The exit code was not read (the Linux launcher has returned by then); B2b measured 3 on a direct start |
| The ELF's self-test through the launcher, each merge | 10,394 lines, 0 `FATAL`, `self-test only: done` |
| The censuses on the merged branch | `os_census --check` and `--boundary`, `width_census --check`, `reloc_census --check`, `reloc_bare --check`, `data_census --strict`, `ledger_check` (242 entries, 0 errors), `ledger_switches --check`, `tu_census --check --strict`, `retire_shells --check --strict`: ok, after the three census files were made again |
| Windows, both presets | build on every merge. The hosted shadow suite (32-bit; 922 `MISMATCHES` lines, every one 0) and the exe host's self-test ran in B2d's worktree on the tree the last merge has, byte for byte (`git diff` of the merge against B2d's tip is empty); they were not run again after it |

**The gate's Windows half (2026-11-21, 18:49 to 19:36, the merged branch's
32-bit build, `bof3x.dll` sha256 `948cc82afeb1...`).** The owner's word was
that the machine was free. Runner `bof3x-runs\lcB\coord\win_gate.py`, C0's
conditions (`c0.py`), outputs in `bof3x-runs\lcB\coord\win\out`.

| What | Result |
|---|---|
| The ten routes on Windows against `cn_<route>_c0x32.sh` | Every route ends with rc 0, 0 `FATAL`, 0 `CRASH`, no compiler beside. `statehash.py diff`: seven identical on every tick (`field_menu` 1,409, `menu_screens` 1,729, `combat` 2,561, `shop` 3,137, `dragonTransform` 4,289, `cutsceneAndNue` 7,425, `whelpBoss` 13,185); `worldMapAndAreaTransition`, `caughFish` and `masterAndManillo` differ on page `0x6C3000` alone, the Direct3D page on which the two references of each of these routes also disagree. `statehash.py check` (against `_c0x32` and `_c0x64`): identical on all ten |
| A quicksave made and restored on Windows (`quick_combat`, a game folder of the round's own) | `quicksave_check.py inproc`: equal, the 300 ticks after the restore are the 300 after the take |
| That file loaded by a second Windows process (`quick_combat_cross`) | `quicksave_check.py cross`: equal on 1,664 ticks from the restore. This is the control for the two rows below: the cross recipe loads a file when the build allows it |
| The Linux-made quicksave on Windows (`quick_combat_cross`) | `quicksave   F7 BISLPS10.DAT refused: another build - made by v0.1.0 (bof3x.dll ad6e260e1d2e...), this is v0.1.0 (948cc82afeb1...); the save half is still a save`. The run goes on to its end: rc 0, 0 `FATAL`, 0 `CRASH`; the file is left as it was (4,786,341 bytes) |
| The Windows-made quicksave on Linux (headless, `coord/b3_cross.sh`) | `quicksave   F7 BISLPS10.DAT refused: another build - made by v0.1.0 (bof3x.dll 948cc82afeb1...), this is v0.1.0 (ad6e260e1d2e...); the save half is still a save`. rc 0, 0 `FATAL`, 0 `CRASH`; the file is left as it was (4,702,989 bytes) |

**A quicksave does not cross the systems, and that is the design.** The
staging's B3 asked for a quicksave made on each system to be *loaded* on the
other. The quicksave's build id holds the hash of the module that made it
(`snapshot.cpp`, `Parse`: "another build"; [`quicksave.md`](quicksave.md)),
and the ELF and `bof3x.dll` are two modules, so each refuses the other's
file, as a 64-bit Windows build refuses a 32-bit one's
([`x64-build-round.md`](x64-build-round.md)). What the gate can ask, and what
the two rows show, is that the refusal is clean on both systems: one log
line that names both builds, no fault, the file untouched, the game playing
on. The owner agreed on 2026-11-21 that no quicksave crosses the systems.
The staging's line is wrong on this point; it is left as written.

**Limits of the Windows half.** The harness measured the time since the last
keyboard or mouse event during every run, and the least was 0 or 1 s in each:
the pointer was moving, a few pixels about eleven times a second (Synergy,
the owner's keyboard and mouse sharing, was running; whether it stopped
during the runs was not measured). No route shows an effect of it. The
quicksave steps ran on the 32-bit build alone. The 64-bit Windows build ran
no route after the last merge (B2d's `shop` on it, before the merge, equals
`_c0x64` on all 3,137 ticks). The save half of a refused quicksave was not
loaded as a save on the other system.

**LC's gate is closed** on these terms: what a headless run on llvmpipe and
the Windows routes can show. What needs a person or a GPU is in the list
below.

**The records.** DIV-0094 and DIV-0238 are amended for the Linux build (the
ELF and its three libraries, a fault's end, the files' case, DXVK's logs,
the stack watch refused under call 2; the movie drawn by the renderer). No
new ledger entry: nothing changes for a Windows player. D433 is the width
and call-shape defects B2a and B2d met; D431's open item is closed by B2c.
`BOF3X_FMV_SHOT_DIR` (B2a) is a developer's switch for a movie frame's
capture, recorded in "B2a as run".

**Not verified, and open:**
- No picture of the Linux build was seen by a person. B2a compared frames by
  hash offscreen; blended pixels differ from the Windows GPU's by at most 2
  in one channel.
- Nothing ran on a GPU or on a real desktop: the WSLg window never took
  focus, so the loop waited at frame 1 there. The window's icon, the
  message box (Wayland wants `zenity` or `kdialog` for it), sound through a
  real device, and speed are unseen. Frost's machine, or a tester's.
- The PC install alone: no disc-built install ran on Linux.
- `Window_DispatchKind`'s stale stack words (D433) were latent on 64-bit
  Windows too; fixed, and only `shop` ran there after the fix.
- B2d's search for a wrong calling convention did not cover pointer types
  filled by an unnamed cast.
- `exe_host_posix.cpp` repeats the portable parts of `exe_host.cpp`.
  `exe_host_steps.h`'s step 2 text disagrees with `bind.cpp`, which reads
  the PE header. The Linux launcher's log says `(bof3x.exe)`. `save_tests`
  does not compile under g++ (the same at `e9c1fb41`). An overflow's report
  has no stack words.
- An empty folder `C:\c` was left by one of B2b's runs.

### D1 as run (2026-11-22, branch `linux/lcC-d1`)

Group D1 of wave C: a native CI gate that keeps the Linux build alive. From
`main` `d56e4097`. Not pushed: **the job has never run on GitHub.**

**What was built.**

- **`build.yml`, a third job `linux-x86_64 (clang)`** on `ubuntu-24.04`. It
  is a product build, so it sits beside the two Windows builds and uploads
  its files, as they do; the host unit suites and the repository gates stay
  `checks.yml`'s. Its steps are one script,
  `.github/scripts/linux-build.sh` (`deps`, `versions`, `configure`,
  `build`, `check`, `selftest`, `tree`), so that D2's release and a run by
  hand do the same thing. In order: the packages (meson, glslang-tools,
  ccache, and SDL3's headers: X11 with XTest, Wayland and libdecor, ALSA,
  PulseAudio, PipeWire, udev, D-Bus, DRM/GBM, EGL, Vulkan); the versions;
  two caches (below); `cmake --preset linux-x86_64`; the build; the ELFs'
  shape; the checks with no game data; `linux_tree.py --check`; the upload
  of `bof3x`, `bof3x-launcher`, the three libraries and the two name lists.
- **The caches.** DXVK-native's libraries, keyed by
  `cmake/dxvk_native.cmake` and `cmake/dxvk_headers.cmake` (which pin and
  build them), SDL3's tag (its headers are DXVK's) and the compiler's
  version line: on a hit the configure passes `BOF3X_DXVK_LIBDIR` and meson
  does not run; on a miss the external project builds them and a step keeps
  meson's install folder for the next run. ccache for every other object,
  restored from the newest entry and saved under the commit.
- **`check`**: the five files are there; both programs are `EXEC` (non-PIE,
  stage LA2), carry RUNPATH `$ORIGIN`, need `libSDL3.so.0` (and `bof3x`
  DXVK's two) and `ldd` finds each in the build folder, nothing "not found";
  `bof3x`'s first `LOAD` segment is at `0x10000000`. A link that ends has no
  undefined name: GNU ld fails an executable's link on one.
- **`selftest`, what needs no game data:** the drawn launcher with no window
  (`--check`, SDL's `offscreen` video and `dummy` sound, no `--game`, no
  `--ini`): a picture of the title page, every tab and the sub-tabs of
  Settings and Controls opened, `report jukebox`, Back, Exit, which saves
  to `--save`; then `tools/key_host`'s `key_tests` and `capture_tests`
  against the SDL source the configure fetched.
- **`checks.yml`'s `os-host`** builds and runs `config_tests` beside
  `os_tests` (the target alone: `save_tests` does not compile under g++,
  "Wave B as run"), and runs the header ratchet beside the boundary gate.
- **The header ratchet of section 3, `os_census.py --headers`.** What
  existed: `--check` refuses a new Windows header row anywhere, and
  `--boundary` refuses `windows.h` outside `src/os` and `src/launcher`.
  Neither sees a POSIX header: the census counts Windows spellings only.
  Measured: with `#include <unistd.h>` added to `src/game/core/sound.cpp`,
  `os_census.py --check` said `ok (1920 sites)` and `--boundary` `ok`.
  `--headers` fails on any operating-system header, Windows' or POSIX's and
  Linux's, in a file of `src/` outside `src/os` and the harness-only files,
  unless `HEADERS_WAITING` lists the pair (file, header); the list may only
  shrink. Today: `ok (32 operating-system includes in 17 files outside
  src/os, all listed; 7 of them in src/game)`. The seven in `src/game`:
  `malloc.h` in `core/sound.cpp` and `platform/music.cpp`, `dinput.h` (under
  `#ifdef _WIN32`) in `platform/d_input.cpp` and `platform/rdata_consts.cpp`,
  and the two `*_win32.cpp` files of `WINDOWS_ONLY`. The scope is wider than
  the game folders on purpose: a POSIX header in `src/common` was as
  invisible as one in `src/game`.
- Which reading was taken: the brief's "self-test with no game data if one
  exists". The ELF's own self-test (`BOF3X_SELFTEST_ONLY=1`) maps Capcom's
  image from `BOF3.exe` or a cache (step 11) and the launcher refuses a
  start with neither ("no game data", "B1 as run"), so it is not in the job.
  `save_editor_check.py` copies a player's saves and an install: not in the
  job either.

**The runner, read and not run.** GitHub's `runner-images`
`images/ubuntu/Ubuntu2404-Readme.md`, image `20261116.376.1` (read
2026-11-22): Clang 16.0.6, 17.0.6, 18.1.3; GNU C++ 12.4.0, 13.3.0, 14.2.0;
CMake 3.31.6; Ninja 1.13.2; pkg-config 1.8.1; Python 3.12.3; no meson and
no glslang. The tree's floors: `cmake_minimum_required(VERSION 3.25)` and
preset version 6 (CMake 3.25); DXVK v3.1.1's `meson_version : '>= 0.58'`
(its `meson.build`); C++20. Ubuntu 24.04's apt has meson 1.3.2 and
glslang 15.1.0 (measured below), so nothing is downloaded beside apt.

**How it was proven here.** No sudo, so a 24.04 root without one:
`ubuntu-base-24.04.5-base-amd64.tar.gz` from cdimage.ubuntu.com (sha256
`e77b6f10...0c86`, checked against its `SHA256SUMS`), entered by user
namespaces (`unshare -r -m`, then `chroot`) in the WSL under
`~/src/lcC-d1`, with what the image holds preinstalled put in first: apt's
`clang-18`, `g++` (13) and `g++-14`, Python 3.12, and Kitware's CMake 3.31.6.
The tree was copied in from the branch's files (`git ls-files -co`). Then
the script's steps in the job's order.

| Step | Result |
|---|---|
| `versions` | `Ubuntu clang version 18.1.3`, `Selected GCC installation: /usr/lib/gcc/x86_64-linux-gnu/14`, cmake 3.31.6, ninja 1.11.1, meson 1.3.2, glslang 11:15.1.0, Python 3.12.3, ccache 4.9.1 |
| `deps` | 54 s |
| `configure`, cold | 38 s; `linux_tree --cmake-list: 794 files for Linux, 12 of the Windows list left out` |
| `build`, cold (12 cores; DXVK by the external project; ccache 890 misses of 902) | 454 s, rc 0 |
| `check` | both `EXEC`, RUNPATH `[$ORIGIN]`, the three libraries found beside them; `first LOAD segment at 0x0000000010000000`; `size`: text 12,137,099, data 14,112, bss 28,105,489 |
| `selftest` | `check (scale 1, renderer software); page -1 sub 0 sheet 0, asked exit, saved the settings`; `saved.ini: 163 lines; title.png: 84052 bytes`; `key_tests` `133780 checks, 0 failed`; `capture_tests` `329 checks, 0 failed` |
| The second run: the keep step's copy, a fresh build folder, `BOF3X_DXVK_LIBDIR` set (a cache hit), ccache warm | configure 65 s, build 15 s (ccache 902 of 902 hits), no `dxvk-native` folder under `_deps`; `bof3x` and both DXVK libraries `md5sum -c` OK against the first run's; `check` and `selftest` as above |
| `tree` (`linux_tree.py --check`) | `837 files, 837 compile for x86_64-linux-gnu, 0 do not; 11 Windows-only files left out`, `--check: ok (0 files wait)`, 152 s |
| `os-host`'s steps, the image's default `c++` (GNU 13.3.0) | `os_tests: 596 checks, 0 failed`; `config_tests` `4455 checks, 0 failures`; `os_census` `--check` ok (1920 sites), `--boundary` ok, `--headers` ok |
| Negative controls | `--headers` with `<unistd.h>` added to `sound.cpp`: rc 1, `src/game/core/sound.cpp:11: include <unistd.h>`; with `<malloc.h>` taken out of `music.cpp`: rc 1, "listed in HEADERS_WAITING but no longer included". `check` with `libdxvk_dxgi.so.0` taken away: "is missing"; with `bof3x`'s ELF type byte made `DYN`: "is not a non-PIE executable" |

The two workflow files parse (`yaml.safe_load`). On Windows, `os_census.py`
`--check`, `--boundary` and `--headers` pass. No source or CMake file was
changed, so the Windows presets were not built again.

**Not verified:**
- **The job on GitHub, all of it.** Above all: that `clang++` on the image
  is clang 18 by that name (the readme lists the versions, not the default
  link; the `versions` step stops with an error if it is not on `PATH`);
  the time on the image's 4 cores (the cold build took 454 s on 12 here);
  the two caches' save and restore by `actions/cache`, and the DXVK key's
  first miss; the artifact's upload (an artifact's zip does not keep the
  executable bit).
- The runner's own extras, which the base root lacks (other toolchains and
  libraries on its paths), and Python 3.13 by `setup-python`: the root had
  3.12.
- The launcher check's picture was not looked at; only its size is checked.
- Nothing here runs the game, a window or DXVK at run time (wave B's
  limits stand).
- The build log holds about 9,000 compiler warnings (`grep -c warning:`),
  the tree's own under `-Wall -Wextra`; the job does not count them.

### D2 as run (2026-11-22, branch linux/lcC-d2)

Group D2 of wave C: the tarball a Linux player uses, made by
`tools/make_release.py`. Beside D1 (the CI job), from `main` `d56e4097`.

**What changed since the staging's row.** The row named "C7's launcher" and
"a start script" (section 6, call 5). Since stage P6 of
[`launcher-portable.md`](launcher-portable.md) (section 14) the drawn
`bof3x-launcher` is the launcher on Linux too, with the Sources page that
runs the importer; the start script was never made and is not wanted. So the
tarball is the zip's layout less the bundled Python: the player's Python
does the setup (section 6, call 3), and the folder is portable as on Windows
(the owner's call 3 of 2026-11-19).

**What was built.**

- **`make_release.py --linux`** (its own function, `main_linux`; the Windows
  path's only change is that `--build`'s default is set after the argument is
  read, to the same `build/`). `python3 tools/make_release.py --linux
  --version V [--build build-linux-x86_64] [--out F]` writes
  `build/release/bof3x-V-linux-x86_64.tar.gz`, one folder of that name
  inside. It downloads nothing. Before it writes, it: checks each
  third-party import the importer reaches against `tools/requirements.txt`
  and `requirements-extra.txt` (or `NOT_BUNDLED`); reads the `RUNPATH` and
  `NEEDED` names of the two programs and three libraries from the files
  (its own ELF reader) and stops unless both programs have `RUNPATH
  $ORIGIN` alone and nothing needs a library outside the folder but glibc
  and the C++ runtime; copies ten notices from the fetched trees and stops
  if one is missing or not the expected text; refuses a build configured
  with `BOF3X_DXVK_LIBDIR` (libraries built elsewhere); runs the zip's
  game-data scan on the stage. The archive sets the modes itself (0755 for
  the programs and the libraries, 0644 for every other file, 0755 for
  folders, owner 0), so that a tarball made on Windows over the WSL's build
  gets the same ones (not run: the tarball below was made in the WSL). It writes the newest `GLIBC_` and `GLIBCXX_` versions the
  binaries need into `README.txt`, in Simplified Technical English as the
  zip's.
- **The launcher finds the system's Python** (`src/launcher/ui/tool_run.cpp`
  `FindRuntime`). It took `runtime/python/bin/python3` whenever
  `runtime/tools/importer.py` was beside it, so a release without a bundled
  Python said "cannot find Python". Now, off Windows only, it takes
  `python3` from `PATH` when that file is not there; the refusal tells the
  player to install Python 3 and the packages, and to read `README.txt`.
  On Windows the condition is a constant false (`kWindowsNames`) and the
  message is the old one; see the measured table for the object compare.
- **[`THIRD_PARTY.md`](THIRD_PARTY.md) section 8**: DXVK-native and what it
  carries, each licence read from the file at the pinned commit's fetched
  tree (DXVK zlib; dxbc-spirv and libdisplay-info MIT, both compiled in;
  Vulkan-Headers, SPIRV-Headers, the MinGW DirectX headers, compiled
  against; libstdc++ and libgcc linked statically into DXVK's libraries by
  its own `meson.build`); SDL3 as a shared library.
- **`README.md`**, a Linux section; **[`releases/README.md`](releases/README.md)**,
  how the tarball is made and a paragraph for the first release notes that
  carry it. `release.yml`'s Linux jobs came after D1's merge (the follow-up
  below).

**The tarball** (`v0.0.0-d2test`, from the `linux-x86_64` preset's build in
the WSL, `~/src/lcC-d2`, a clean configure that fetched SDL3, Dear ImGui and
DXVK; made there by `python3 tools/make_release.py --linux --version
v0.0.0-d2test --build build-linux-x86_64 --out
~/bof3x-runs/lcC-d2/bof3x-v0.0.0-d2test-linux-x86_64.tar.gz`):
38,692,005 bytes (`ls -l`), 112 files and 114,859,152 bytes unpacked
(`find . -type f | wc -l`, `du -sb`), 22 folders.

| Kind | Files | Bytes (`stat -c %s`, `find -printf %s`) |
|---|---:|---:|
| programs: `bof3x`, `bof3x-launcher` | 2 | 75,964,256 (56,689,376 and 19,274,880) |
| libraries: `libSDL3.so.0`, `libdxvk_d3d11.so.0`, `libdxvk_dxgi.so.0` | 3 | 23,907,352 (9,709,384, 8,462,704, 5,735,264) |
| top-level text: `README.txt`, `LICENSE`, `THIRD_PARTY.md`, the two names files | 5 | 85,560 |
| `licenses/`: dear-imgui, droid-sans (2), tex-gyre-pagella (4), dxvk, dxbc-spirv, libdisplay-info, vulkan-headers (3), spirv-headers, mingw-directx-headers (2), sdl3 | 17 | 93,296 |
| `runtime/tools/`: the 35 reached tools and `requirements.txt` | 36 | 1,908,773 |
| `runtime/`: `symbols.toml`, `fixtures.toml`, `tables.toml` | 3 | 8,172,724 |
| `runtime/fixtures/`, `recipes/`, `exe_maps/`, `data/`, `src/` | 10, 24, 7, 3, 2 | 1,101,419, 2,927,885, 655,045, 18,634, 24,208 |

`tar -tvzf`: 5 entries `-rwxr-xr-x` (the two programs and three
libraries), 107 `-rw-r--r--`, 22 folders `drwxr-xr-x`. `readelf -d`:
`bof3x` NEEDED `libdxvk_d3d11.so.0 libdxvk_dxgi.so.0 libSDL3.so.0
libstdc++.so.6 libm.so.6 libgcc_s.so.1 libc.so.6`, RUNPATH `$ORIGIN`;
`bof3x-launcher` `libSDL3.so.0` and the same four system names, RUNPATH
`$ORIGIN`; `libSDL3.so.0` `libm`, `libc`; `libdxvk_d3d11.so.0`
`libdxvk_dxgi.so.0 libc.so.6 ld-linux-x86-64.so.2`; `libdxvk_dxgi.so.0`
`libm`, `libc`, `ld-linux`; none names `libvulkan` (DXVK holds the strings
`libvulkan.so.1` and `libvulkan.so`, `strings`). Version needs (`objdump
-T`): `GLIBC_2.43` from `bof3x-launcher` and `libSDL3.so.0`, `GLIBC_2.38`
from the rest; `GLIBCXX_3.4.31` from the two programs. The build tree's path
appears in `bof3x` as source file names (`strings`: `src/hook/bind.cpp` and
others, debug and message text), never as a search path.

**The proof**, in a clean folder on ext4 (`~/bof3x-runs/lcC-d2/clean`), every
command under `env -i HOME=... PATH=/usr/bin:/bin SDL_VIDEODRIVER=offscreen
SDL_AUDIODRIVER=dummy`, so no `LD_LIBRARY_PATH` and no path to the build
tree. The source: the main checkout's `bof3` folder copied to
`~/bof3x-runs/lcC-d2/pc` (ext4) without its `CDImage` folder. Script:
`proof_d2.sh` in the session's scratch folder.

| Step | Result |
|---|---|
| `ldd` on the five binaries | every library the folder holds resolves to the unpacked folder (`libdxvk_d3d11.so.0`, `libdxvk_dxgi.so.0`, `libSDL3.so.0` for `bof3x`; `libSDL3.so.0` for the launcher); the rest to `/usr/lib/x86_64-linux-gnu`. `ldd libdxvk_d3d11.so.0` by itself says `libdxvk_dxgi.so.0 => not found` (a library has no RUNPATH); in a run `bof3x` has loaded it first, by its own NEEDED, and the script checks that order |
| `bof3x-launcher --list` | rc 0, the settings listed |
| `bof3x-launcher --check --capture title.png` | rc 0, `check (scale 1, renderer vulkan); page -1 ... saved nothing`; the title page drawn into the PNG (not committed) |
| No `python3` on `PATH` (`PATH=/nonexistent`), `--check --start` | the Sources page: `The importer cannot run.` and `The importer cannot find Python (none on PATH). Install Python 3 and the packages in requirements.txt. README.txt tells how.` |
| No venv, the system's `python3` (3.14.4, without Pillow or capstone), `--check --start --script "wait 60; press Folder...; pick <pc>; press Add; wait 600; report sources"` (a second unpacked copy) | `sources list`, `sources add`, `setup --json-progress`, `stale` started on `runtime/tools/importer.py`; `The install is ready.` |
| `python3 -m venv runtime/python` | rc 1 (`ensurepip` missing: Ubuntu's `python3-venv` is not installed here) |
| `uv venv runtime/python`; `uv pip install --python runtime/python -r runtime/tools/requirements.txt` | CPython 3.14.4; `capstone==5.0.9`, `pillow==12.0.0` installed (capstone's own `__version__` says 5.0.7) |
| `runtime/python/bin/python3` replaced, in a third copy, by a script that writes down its start | the launcher started it for `importer.py sources --home ...`: the venv's Python comes first |
| The same Sources script with the venv | rc 0 in 12 s; `Recognised: PC, the 2001 Chinese release`, `No ffmpeg found: skipping the PC logo movie's repair (DIV-0001)`, the executable's tables and language layer zh-CN, `The install is ready.`; `cache/` 322 MB, `game/` 527 MB, `sources.toml` and `setup.log`, all in the unpacked folder |
| `runtime/python/bin/python3 runtime/tools/importer.py verify --cache cache --overlays game/DAT` | rc 0: `995 containers held, 0 problem(s)`; `742 of 742 containers byte-identical`; the image is the PC source's `.data`, 638,976 bytes compared |
| `BOF3X_SELFTEST_ONLY=1 bof3x-launcher --no-config` (from an empty working folder; the game folder found as `<folder>/game`) | rc 0; `.data from <folder>/cache (a PC install ...); BOF3.exe is not read`; `self-test only, game process exit code 0`; `bof3x.log` beside the ELF: 10,400 lines, 0 `FATAL`, 0 `CRASH`, the last `self-test only: done, ending the process` |
| What the runs wrote | in the unpacked folder only: `bof3x.log`, `setup.log`, `sources.toml`, `cache/`, `game/`, `runtime/python/` (the venv) |

**Windows, unchanged** (the branch's worktree, llvm-mingw):

| Check | Result |
|---|---|
| `cmake --preset i686 && cmake --build build`; `cmake --preset x86_64 && cmake --build build-x86_64` (SDL3 and Dear ImGui from the main checkout's fetched trees) | both whole, no error |
| `tool_run.cpp.obj` built from `origin/main`'s source and from the branch's, each width, `codegen_same.py` and `--by-function` | 0 differ: 32-bit 92 functions, 50 data and 3 bss symbols; 64-bit 93, 19 and 3 |
| The zips by `origin/main`'s `make_release.py` and by the branch's, from the same `build` and `build-x86_64` (`--version v0.0.0-d2test`) | each pair 1,219 entries in the same order, the same names, sizes and methods (233,816,025 and 229,833,853 bytes unpacked); one entry's CRC differs in each pair, numpy's `dist-info/RECORD`, in two lines: the hashes of the console-script launchers pip writes (`bin/f2py.exe`, `bin/numpy-config.exe`) and the script then deletes, which pip makes anew on every install |

**The follow-up after D1's merge (2026-11-22, on `linux/lcC` `14f58a07`).**

- **`release.yml`, two jobs; the `release` job is not changed** (`git diff`
  against `origin/main` deletes no line of the file). Job `linux`
  (`ubuntu-24.04`): the tag step as the `release` job's (the same `TAG`,
  `VERSION`, `REF`), checkout of `REF`, Python 3.13, then D1's script:
  `deps`, `configure` with `BOF3X_DXVK_LIBDIR` empty, `build`, `check`;
  then `python tools/make_release.py --linux --version "$VERSION" --build
  build-linux-x86_64 --out "build/release/bof3x-$TAG-linux-x86_64.tar.gz"`
  and the tarball handed on as an artifact. Job `linux-attach` needs both
  and runs when `release` succeeded, whatever `linux` did: with the tarball
  it runs `gh release upload "$TAG" ... --clobber` onto the draft or onto
  "latest"; without it, it takes `bof3x-latest-linux-x86_64.tar.gz` off
  "latest" (an older build's tarball beside new zips would mislead) and
  fails. **Why the release goes out without Linux:** the file's own rule is
  that the Windows zips are the player's default and publishing is the
  owner's click; the Linux build is new and unplayed, so its failure should
  not hold the zips back, and the red run says it failed. The notes of
  "latest", which the `release` job writes, are not changed and do not name
  the tarball.
- **DXVK's notices against the cache.** `build.yml`'s Linux job passes
  `BOF3X_DXVK_LIBDIR` on a cache hit, and `make_release.py` refuses such a
  build. The release job has no DXVK cache: DXVK-native is built from the
  pinned commit in every release run, and the notices are copied from the
  tree that build fetched. The check was not loosened.
- **The glibc floor, measured.** The tip's tree built in an Ubuntu 24.04
  root of D2's own (a copy of D1's `~/src/lcC-d1/noble`, entered the same
  way; `~/src/lcC-d2/enter.sh`, the tree in `~/src/lcC-d2/work/repo`, D1's
  ccache copied): `versions`, `configure` (`BOF3X_DXVK_LIBDIR:PATH=` empty,
  DXVK built by the external project: `_deps/dxvk-native/lib/libdxvk_*.so.0.30101`),
  `build` 75 s, `check` ok (first `LOAD` at `0x10000000`). `ldd (Ubuntu
  GLIBC 2.39-0ubuntu8.12) 2.39`, Python 3.12.3. `make_release.py --linux
  --version v0.0.0-d2noble` there: 112 files, 39,119,850 bytes, sha256
  `5df6e145...b493d0e6`, the same NEEDED and RUNPATH as above, game-data
  scan clean, **`needs at least GLIBC_2.38, GLIBCXX_3.4.31`**, and
  `README.txt` says "glibc 2.38 or newer and the GNU C++ library with
  GLIBCXX_3.4.31 or newer". Per binary (`objdump -T`): all five
  `GLIBC_2.38`; `GLIBCXX_3.4.31` from the two programs.
- **That tarball on 26.04**, the proof above again (`proof_noble.sh`, the
  same steps): modes 5 `-rwxr-xr-x`, 107 `-rw-r--r--`, 22 folders; 112
  files, 116,502,147 bytes; `ldd` under `env -i` finds the three libraries
  in the unpacked folder and the rest in `/usr/lib/x86_64-linux-gnu`;
  `--list` and `--check` rc 0; no `python3` on `PATH`: the refusal; the
  system's `python3`: "The install is ready."; the venv's wrapper started
  for `importer.py sources`; with the `uv` venv the Sources script rc 0 in
  5 s, "The install is ready."; `verify` rc 0, `995 containers held, 0
  problem(s)`, `742 of 742 containers byte-identical`; `BOF3X_SELFTEST_ONLY=1
  bof3x-launcher --no-config` rc 0, 10,400 log lines, 0 `FATAL`, 0 `CRASH`,
  `self-test only: done, ending the process`.
- **One line of the tarball's `README.txt` changed after these proofs:** its
  `tar -xzf` example names `bof3x-*-linux-x86_64.tar.gz`, since "latest"'s
  file is `bof3x-latest-linux-x86_64.tar.gz` while its folder and version
  are `latest-<commit>`. Checked by a `--version latest-14f58a07` run of
  the script: the line reads so, the folder is
  `bof3x-latest-14f58a07-linux-x86_64`.

**Not verified:**
- The title screen, the window, a picture, sound, a pad: no person and no
  GPU; the launcher's title page was drawn by `--check` into a PNG only.
  The staging's "setup, then the title screen" stands here as setup, verify
  and the ELF's self-test.
- A system older than the build's. The 26.04-built tarball ran on the
  machine that built it; the 24.04-built one (the follow-up below) ran on
  26.04, a newer system, which is a player's usual case. No system at the
  floor itself (glibc 2.38), no other distribution, SteamOS or Deck was
  tried. **The tarball needs the build machine's glibc or newer.**
- A disc source on Linux through the tarball (the PC folder alone), and so
  the paths that import Pillow and capstone: the PC setup ran to "The
  install is ready" on the system's Python with neither installed.
- `python3 -m venv` with `python3-venv` installed: this WSL lacks the
  package (no `ensurepip`), so the README's first way failed as it says it
  will without the package; the `uv` way was run.
- **The release jobs have never run on GitHub.** Not run: the runner's
  `deps`, its clang and Python 3.13 under `make_release.py`, a cold DXVK
  build on the runner's 4 cores, the artifact's hand-over, `gh release
  upload` onto a draft found by its tag, `--clobber` on "latest", and the
  failure path's `gh release delete-asset`. The workflow was parsed
  (`yaml.safe_load`), and every expression and `if:` read by hand;
  `actionlint` is not installed here.
- The Linux binaries carry no release tag: `BOF3X_VERSION_TAG` stamps the
  Windows files alone (`CMakeLists.txt`), and the Linux job does not pass it.
  The tag is in the tarball's name and its `README.txt`.
- libdisplay-info's commit (`275e6459`, the branch `windows` of DXVK's fork)
  against the commit DXVK's tree records for it: read from the clone's
  files, not by `git`.

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
3. **Tier 3:** the pages in a portable toolkit, estimated and staged in
   [`launcher-portable.md`](launcher-portable.md) (2026-11-19). IDEAS I40 (the
   importer in C++) shrinks it, since the Sources page is then no longer a
   front end to a Python process.

## 6. For the owner

"The owner" in this document is Kevin (kerokline), as everywhere in this
repository. The **Decided** marks below are frost's (raulcorreia7, who runs
this round on his fork), made in his sessions on 2026-11-11 so the work could
proceed; they stand as the round's working answers, and each is still the
owner's to confirm or change at the merge of the unit that depends on it.

1. **The second renderer's API.** Proposed: SDL3's GPU API, since SDL3 is
   already built in and it reaches Vulkan on Linux and Direct3D 12 or Vulkan
   on Windows from one backend. The alternative is OpenGL 3.3, which is
   simpler and reaches older hardware. Either needs the shaders in a second
   form; how is LA4's finding.
   **Measured 2026-11-11 (`tools/linux_probe/dxvk_probe.cpp`): a third road,
   DXVK-native** - DXVK v3.1.1 built as Linux libraries (`meson
   -Dnative_sdl3=enabled`; zlib licence, permissive), Direct3D 11 on Vulkan
   with an SDL3 window as the swap chain's `HWND`. The probe, a native x86-64
   ELF, makes the device and swap chain as `InitOnFiber` does, creates the
   present shaders from `shaders_compiled.h` (Windows' compiler's DXBC,
   unchanged), draws a 320 x 240 picture doubled to 640 x 480 and reads it
   back: 0 of 307,200 pixels differ; 19,557 presents a second on an RTX 5090.
   It keeps `render_d3d11.cpp` and the shaders as they are, so no second
   backend and no second shader form. `render_d3d11.cpp` against DXVK's
   headers (`clang++ -fsyntax-only`): every Direct3D 11 and DXGI name
   resolves; what does not is Win32 alone - the heap (11 sites), environment
   (5), thread id (5), last error (3), `GetClientRect` (2), the render fiber
   (5) and one file write - the OS seam LB1 and LB6 already plan.
   `D3DCompile` has no native implementation, so `BOF3X_PIXEL_OFFSET` would
   refuse there. **Decided (frost, raulcorreia7, 2026-11-11): DXVK-native for the
   Linux build; Windows keeps Microsoft's Direct3D 11, with no DXVK switch**
   - on Windows DXVK would add a Vulkan 1.3 requirement and DLLs to ship for
   no gain a player sees; the native build and Proton already give a Vulkan
   comparison when one is wanted. `render_d3d11.cpp` stays the one renderer
   source.
2. **Whether Direct3D 11 stays on Windows** once the second backend passes.
   Proposed: yes, as the default there for a release, with the new backend a
   switch; then the owner's call.
3. **The importer on Linux.** **Decided (2026-11-11): the system's Python
   with a requirements file** for the first native release; I40 later. The
   importer already runs on Linux's own Python (the PSX media round's M2
   set-up ran on this machine's).
4. **DirectInput.** **Decided: retired for SDL3 on both systems** - one
   input path to test. A ledger entry where a player could tell.
5. **The release's form.** **Decided: a plain tarball first** - the ELF,
   DXVK-native's `libdxvk_d3d11.so` and `libdxvk_dxgi.so`, a start script that
   runs the importer's `setup` and the game. The Deck takes it as a non-Steam
   game. No native launcher yet (section 5, stand-in 2).
6. **The order.** **Decided: the minimum native path now**, in gated units on
   one branch, each proven on Windows before the next (section 4's method):
   L1 the OS seam; L2 the window, keyboard and pad on SDL3 and the movie
   drawn by the renderer; L3 the fibers and the crash reporter's catcher; L4
   the ELF link (the low-memory image from LA2, `ms_abi` where LA3 says,
   DXVK-native fetched pinned like llvm-mingw); L5 the proof (routes against
   the Windows 64-bit build), the CI job and the tarball.

## 7. Not verified

- Tier 1 on a real GPU, with a person: section 2a ran on a software
  renderer and names each row it left.
- That D411 shows on a hardware Vulkan driver as it does on llvmpipe. The
  compiler is the same on both, and the native DLL alone cured it, so it
  should; it was not seen.
- That the 32-bit build runs without `PROTON_USE_WOW64=1` where 32-bit
  graphics drivers are installed.
- `mmap_min_addr` read 65,536 on this machine (section 4, LA2, `/proc/sys/vm/mmap_min_addr`;
  an earlier read in WSL2 gave the same). Not read on the other
  distributions players use or on SteamOS. Answered here: an ELF linked high
  can leave `0x400000`..`0x98F000` free (or the base can move to `0xA00000`);
  not known: that the real engine's ELF fits below `0xA00000`.
- What the 65 game files outside `platform` use from `windows.h`: answered in
  section 4 (24 nothing, 16 environment switches, 1 `_stricmp`, 7 services,
  17 harness files). Not accounted for: 132 `#include <windows.h>` lines by
  `grep`, 130 include rows in the census.
- Which side of the renderer's interface `src/game/platform/d3d.cpp` is on.
- A crash switch for the checklist's row 11: `BOF3X_CRASH_TEST=<seconds>`, named in
  the header comment of `src/hook/crash.cpp`; a search for `CRASHTEST` had
  missed it. Not run.
- That SDL3's GPU API covers what `render_d3d11.cpp` does (the present pass,
  the readbacks, the texture page uploads). LA4 reads it against SDL's
  documentation before call 1 is made.
- The Win64 asm count is 10 of 24 live x86-64 blocks (section 4, LA3). Not run:
  a full task switch under `ms_abi` on Linux; the whole tree under
  `ms_abi` (24 files and the 11 asm files were compiled); a call across the
  SDL or DXVK boundary; the probes on a kernel other than 7.3.1.

## 8. Not in these tiers

- A native launcher (tier 3; [`launcher-portable.md`](launcher-portable.md)
  stages it, and its stage P0 is this round's group C7) and the importer in
  C++ (I40).
- ARM64 and macOS.
- The 32-bit build on Linux. The native build is the 64-bit host; the 32-bit
  build reaches Linux through tier 1.
- Any change to what the game does. A native build adds no behaviour; what
  stage LB changes on Windows is ledgered there.
