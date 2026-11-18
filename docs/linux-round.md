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
and played to its end once, and was skipped once. **Not reported on:** a
close of the window during the movie, a pad-bound action,
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

- A native launcher (tier 3) and the importer in C++ (I40).
- ARM64 and macOS.
- The 32-bit build on Linux. The native build is the 64-bit host; the 32-bit
  build reaches Linux through tier 1.
- Any change to what the game does. A native build adds no behaviour; what
  stage LB changes on Windows is ledgered there.
