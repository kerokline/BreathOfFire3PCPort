# The Vulkan / Direct3D 11 toggle: a round, staged (IDEAS I63)

**Status: STAGED, 2026-11-18. Not launched; nothing built.** The owner
asked for it the same day, while the Linux round's wave 2 ran. It launches
at the owner's word, after wave 2's R9 is on `main` (R9 edits the files this
round edits). Sections 2 and 2a are measured (stage V0 ran the same day and changed the
design); sections 3 to 6 are a plan, and section 7 lists what is still
not checked.

## 1. What is asked, and the road proposed

A player on Windows chooses the graphics API: Direct3D 11 (today's, and
still the default) or Vulkan.

**Proposed road: DXVK as the Direct3D 11 implementation, loaded by choice.**
DXVK is Direct3D 11 written on Vulkan (zlib licence). The renderer keeps its
one source, `render_d3d11.cpp`; what changes is which `d3d11.dll` it asks
for its device. The Linux native build already takes this road
([`linux-round.md`](linux-round.md) 6, call 1: DXVK-native, measured
2026-11-11, 0 of 307,200 pixels different on the present pass).

This changes the second half of that call. frost's working answer was
"Windows keeps Microsoft's Direct3D 11, with no DXVK switch", since a
switch adds a Vulkan 1.3 requirement and DLLs to ship for no gain a player
sees. Both costs are real and are in section 5. What the switch buys:

- **The Linux build's draw path under the owner's eye on Windows.** The
  native build's renderer is `render_d3d11.cpp` through DXVK. With the
  toggle, every route, shot and live check on the Windows machine can run
  through the same translation. It does not cover DXVK-native's window
  code, which is Linux's alone.
- **A second path for a machine whose Direct3D 11 driver draws wrong**, and
  a comparison when a report says "the picture is wrong on my card".
- No second backend and no second form of the shaders, which the other
  roads need.

**The other roads, not proposed** (IDEAS I63 has them): a backend on SDL3's
GPU API or on Vulkan by hand. Either is a second renderer to keep equal to
the first - `render_d3d11.cpp` is 1,219 lines and `satpixie.cpp` 398, with
ten shaders - and gives a player nothing this road does not. They stay in
I63 for the day DXVK is not enough.

## 2. Measured for this staging (2026-11-18)

- **The game's DLL imports one Direct3D name.** `llvm-objdump -p
  build/bof3x.dll`: from `d3d11.dll`, `D3D11CreateDeviceAndSwapChain` alone;
  from `D3DCOMPILER_47.dll`, `D3DCompile` alone; nothing from `dxgi.dll`
  (the factory is reached through the device, `render_d3d11.cpp:923`).
  `bof3x.exe` and the launcher import none of the three.
- **One call makes the device:** `render_d3d11.cpp:907` (and its retry,
  :912), feature levels 11_0, 10_1, 10_0 (:902). The swap chain is `DISCARD`,
  `SEQUENTIAL` or `FLIP_DISCARD` by option (:894..900); the present is
  `Present(vsync ? 1 : 0, 0)` (:863, :877).
- **The Direct3D surface:** 147 lines of `render_d3d11.cpp` and 40 of
  `satpixie.cpp` name a Direct3D 11 or DXGI type or constant
  (`grep -cE "ID3D11|IDXGI|D3D11_|DXGI_|D3D_"`); no other file in `src`
  does.
- **The 32-bit game has 2 GB of addresses.** `BOF3.exe` and `bof3x.exe`:
  `Characteristics 0x10f`, so the large-address bit (0x20) is not set.
- **SDL's video code loads `dxgi.dll` itself** on Windows
  (`build/_deps/sdl3-src/src/video/windows/SDL_windowsmodes.c`). SDL's video
  is off in today's build (`CMakeLists.txt:131..132`; the one `SDL_Init` is
  the pad's, `pad_sdl.cpp:86`). Wave 2's R6 turns it on.

## 2a. Stage V0 as run (2026-11-18, branch `render/vulkan-toggle-v0`, commit `2a1f1c2`)

A standalone probe, `tools/dxvk_win_probe/`, at 32 bits (large-address bit
off, as the game's) and 64: the same work through Microsoft's Direct3D 11
and through DXVK v3.1.1, each result one printed line. No game ran. The
outputs are in `C:/Users/kerok/bof3x-runs/vk0/out/`. The machine: an RTX
3060 Ti (driver 610.88) and an AMD integrated GPU, Vulkan 1.4 on both.

**The archive:** `dxvk-3.1.1.tar.gz` from the project's GitHub releases,
18,041,512 bytes, SHA-256
`40565b4a724aadc4433fa4e010b4b23916d9b1f1baeee64e17186db94f54e608`.
Direct3D 11 needs two files of it: `d3d11.dll` (8.2 MB at 32 bits, 7.5 MB at
64) and `dxgi.dll` (5.9 MB, 5.4 MB). None is signed. **The archive holds no
licence file.**

**It works, and the picture is the same.** The device call answers `S_OK`
at feature level 11_0 under both at both widths. `DISCARD`, `SEQUENTIAL`
and `FLIP_DISCARD`, `MakeWindowAssociation` (DXVK ignores its flags),
`Present(0,0)` and `Present(1,0)` all answer `S_OK`. Read back through a
staging texture: the present pass at point and at linear, the game's scene
shader's dual-source variant, and a second dual-source state - 0 of 307,200
pixels differ between Microsoft's and DXVK's, at both widths, on each GPU.
Between the two GPUs the linear pass differs on 5,203 pixels by one step
and a dual-source draw on 108,767 by up to 61: a difference of cards, under
either API.

| | Microsoft, 32 | DXVK, 32 | Microsoft, 64 | DXVK, 64 |
|---|---|---|---|---|
| Load to first present | 1109 ms | 2471 ms | 1852 ms | 2291 ms |
| `Present(0,0)` x 600, mean / worst | 0.35 / 0.99 ms | 0.60 / 10.8 ms | 0.43 / 57.4 ms | 0.61 / 33.9 ms |
| Private bytes after 600 presents | 27 MB | 183 MB | - | - |
| Virtual size after 600 presents | 440 MB | 608 MB | - | - |

**Four findings the design did not have.**

1. **The load-order trap is real, and there is a cure that does not depend
   on the order.** With the system's `dxgi.dll` loaded first (as SDL's
   video start-up loads it), DXVK's `d3d11.dll` binds to it and the device
   call fails with `E_NOINTERFACE` after 5 to 8 seconds. Passing the adapter
   from DXVK's own `CreateDXGIFactory1` makes the device in either order.
   With DXVK's pair loaded first, a later load of `dxgi.dll` by name -
   SDL's - gets DXVK's, and its factory works there.
2. **The fall-back as section 3 had it is not safe.** Once DXVK's pair is in
   the process, `LoadLibraryExW(L"d3d11.dll", LOAD_LIBRARY_SEARCH_SYSTEM32)`
   returns DXVK's module, so a fall-back would get DXVK again; Microsoft's
   `d3d11.dll` by full path beside DXVK's `dxgi.dll` is an access violation.
   What works: `FreeLibrary` of both DXVK modules, then the load from
   System32 (measured with a forced failure). The same shows that the flag
   is no hardening by itself: the renderer must check the path of the
   module it got.
3. **In a full 32-bit process DXVK fails badly, not cleanly.** With address
   space reserved beforehand to stand in for the game: it works at 1400 MB;
   at 1500 MB NVIDIA's Vulkan driver is no longer listed, DXVK takes the
   other GPU without a word and then faults (0xC0000005); at 1600 MB the
   device call answers `E_FAIL`. Microsoft's works to 1600 MB. The real
   game's use at the renderer's start is not measured.
4. **DXVK writes a shader cache** under `%LOCALAPPDATA%\dxvk` by default
   (`DXVK_SHADER_CACHE_PATH` moves it; tested), and it reports the GPU
   under another vendor's id by default ("Hiding actual GPU").

**Not run in V0:** the modules in the process at the renderer's start in a
real game (hosted and exe host); a build of DXVK from source, so call 2's
signing road is not timed.

## 3. The design

**The switch.** `BOF3X_RENDER_API=d3d11|vulkan`, set by the launcher from an
ini key, as `background=` is (`config.cpp:241`, :432, :645). `d3d11` is the
default. The name says the API a player knows, not the library.

**The load** (changed by V0, section 2a). The import of `d3d11.dll` goes;
the renderer finds `D3D11CreateDeviceAndSwapChain` at run time and checks
the full path of the module it got:

- `d3d11`: the module from System32, and a `Fatal` if the one loaded is
  from anywhere else.
- `vulkan`: DXVK's `dxgi.dll`, then its `d3d11.dll`, by full path from a
  folder of our own beside the engine (`dxvk\x32`, `dxvk\x64`), before
  SDL's video starts; the device is made on the adapter that DXVK's own
  `CreateDXGIFactory1` gives, so that the order of the loads cannot decide
  the result. `DXVK_SHADER_CACHE_PATH` and `DXVK_LOG_PATH` point into the
  home folder.
- The adapter DXVK offers is compared with the one Direct3D 11 would use
  (by its LUID): if DXVK is on another GPU, that is a failure to start, not
  a picture from the wrong card.

**When Vulkan cannot start** (the files are not there, no driver, another
GPU, the device call fails - allow 8 seconds): the renderer logs why, frees
both DXVK modules, loads System32's `d3d11.dll`, checks its path, and makes
the Direct3D 11 device; the launcher's next start says so in one line. A
switch `BOF3X_RENDER_API=vulkan!` (the harness's) makes the same failure a
`Fatal`, so that a proof run cannot pass on the wrong API. A fault inside
DXVK or a driver cannot be caught this way: that is why the 32-bit game is
call 3.

**The shaders** do not change: DXVK takes the same compiled bytecode
(`shaders_compiled.h`) that Direct3D takes. `D3DCompile`, for
`BOF3X_PIXEL_OFFSET`, stays Microsoft's compiler on Windows under both.

**The launcher.** One box on the Settings page: "Graphics API", with
"Direct3D 11" and "Vulkan". Both are always listed. "Vulkan" is grey with a
reason when DXVK's files are not installed, and under Wine or Proton, where
Direct3D 11 is DXVK already. Text in Simplified Technical English.

**Linux native:** no toggle. That build has Vulkan through DXVK-native and
nothing else. The key is read and `d3d11` there is a clear refusal.

## 4. The stages

| Stage | What | Size | Proof |
|---|---|---|---|
| V0 | Measure, no change in behaviour: the modules loaded at the renderer's start (hosted and exe host, both widths); a pinned DXVK release's Windows DLLs loaded by hand in a scratch build; whether its 32-bit build makes a device inside the hosted `BOF3.exe`'s 2 GB; what it asks of the driver | a probe and a doc section | the numbers, written here |
| V1 | The run-time load and `BOF3X_RENDER_API`, with the fall-back and the `!` form | about 100 to 150 lines in `render_d3d11.cpp` and one new file; `CMakeLists.txt` loses `d3d11` from the link line | both presets; the hosted suite; `d3d11` shots byte-equal to the base's |
| V2 | The files: a tool that fetches the pinned DXVK release by hash into `dxvk\`, the build and `release.yml` use it; `THIRD_PARTY.md` carries the notice | a script of about 80 lines, the workflow, the doc | the hash checked on every fetch; a clean build has the folder |
| V3 | The launcher's box, the ini key, the grey rule, the fall-back notice | about 60 lines, as `background=` | the launcher's own tests; the owner's look |
| V4 | The comparison: every recipe's shots under both APIs, at `point` and `linear`, with `satpixie`, with the in-between frame, the frame save and the snapshot's picture; frame times for both | harness time, no code but a compare script | section 6 |
| V5 | The ledger entry, `launcher-settings.md`, `render-backend.md`, the release notes, IDEAS I63's outcome, `linux-round.md` call 1 | docs | `ledger_check.py` |

V0 changes no behaviour and can run beside the Linux round. V1 to V3 are
one agent each or one agent in order; they share `render_d3d11.cpp` with
R9, so they start from a tree that has R9. V4 is the coordinator's and
needs the machine hands-off.

## 5. What it costs, and the owner's calls

1. **Ship DXVK's DLLs, or fetch them on demand.** Shipping adds four files
   (`d3d11.dll` and `dxgi.dll` at each width; a few MB each - not
   measured) to every release, built by someone else. Proposed: ship them,
   fetched pinned by hash in `release.yml`, so that the option works with
   no download by the player. The engine/data split is not touched: DXVK is
   third-party code under a permissive licence, named in `THIRD_PARTY.md`,
   and nothing of it enters the repository.
2. **Signing.** The release signs our files
   ([`LICENSING.md`](LICENSING.md), `release.yml`). DXVK's DLLs come
   unsigned. Sign them with our certificate (we then vouch for a binary we
   did not build), or ship them unsigned (Defender has removed unsigned
   local builds of ours before). Proposed: build DXVK from its pinned
   source in CI and sign that, if V0 shows the build is an hour or less;
   otherwise ship the project's own release binaries unsigned and say so.
3. **The 32-bit game.** V0: with 1500 MB of a 2 GB process taken, DXVK
   moves to the other GPU and faults. Proposed: Vulkan on the 64-bit build
   alone, the box grey on the 32-bit one with that reason; the large-address
   bit is not set for it.
4. **Is Vulkan ever the default on Windows.** Proposed: no. Direct3D 11
   needs nothing installed and is the path every route was proven on. This
   is `linux-round.md` call 2 turned round.
5. **A ledger entry.** A player who picks Vulkan gets the same picture by
   intent; the option itself, the fall-back and the hardened load are new
   behaviour. Proposed: one entry, numbered at the merge.

## 6. The gate

- Every stage: both presets build; the hosted shadow suite, the exe-host
  run and the moved-base run; `data_census --strict`, `reloc_census
  --check`, `ledger_check.py`.
- V1: with `d3d11`, the ten state-hash routes equal the references, and a
  recipe's shots are byte-equal to the base build's.
- V4, with `vulkan!`:
  - the ten state-hash routes equal the references (the renderer writes no
    game memory but the rescale hook's, `d3d.cpp:911..925`);
  - shots at `point`: byte-equal to Direct3D 11's, or every different pixel
    counted and explained. At `linear` and under `satpixie` a rounding
    difference of one step is expected and is counted, not waved through;
  - the frame save, the snapshot's picture and `fb_grab.cpp`'s readback
    return the same bytes;
  - a quicksave made under one API loads under the other;
  - focus loss, a resize, each window mode, vsync on and off.
- The owner's eye: a play of a field, a battle, the world map and a movie
  under Vulkan; the launcher's box; the notice after a forced fall-back.

## 7. Not verified

- Settled by V0 (section 2a): the Windows DLLs at both widths and their
  sizes, the three swap effects, `MakeWindowAssociation`, dual-source
  blending and the staging readback against Microsoft's, and that Windows
  gives the `dxgi.dll` already loaded to DXVK's `d3d11.dll`.
- The Vulkan version DXVK v3.1.1 asks: its log says it only when the check
  fails, and this machine has 1.4.
- The game's own address use at the renderer's start, at 32 bits.
- That `LOAD_LIBRARY_SEARCH_SYSTEM32` is right inside the hosted
  `BOF3.exe` under Wine, where `d3d11.dll` may be an override in the
  prefix. Tier 1 ([`linux-round.md`](linux-round.md) 2) must still draw.
- Frame times. DXVK compiles pipelines at first use; a hitch at a first
  draw of a state is possible and is V4's to measure.
- The in-between frame's presents (DIV-0230) under DXVK's present timing.
- What R6's SDL window and R9's header work leave in the files this round
  edits: the line numbers above are `main`'s at `5683387a` plus R7.
