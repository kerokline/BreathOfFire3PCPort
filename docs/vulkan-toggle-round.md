# The Vulkan / Direct3D 11 toggle: a round, staged (IDEAS I63)

**Status: STAGED, 2026-11-18. Not launched; nothing built.** The owner
asked for it the same day, while the Linux round's wave 2 ran. It launches
at the owner's word, after wave 2's R9 is on `main` (R9 edits the files this
round edits). Section 2 is measured; sections 3 to 6 are a plan, and
section 7 lists what the plan assumes and nobody has checked.

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

## 3. The design

**The switch.** `BOF3X_RENDER_API=d3d11|vulkan`, set by the launcher from an
ini key, as `background=` is (`config.cpp:241`, :432, :645). `d3d11` is the
default. The name says the API a player knows, not the library.

**The load.** The import of `d3d11.dll` goes; the renderer finds
`D3D11CreateDeviceAndSwapChain` at run time:

- `d3d11`: `LoadLibraryExW(L"d3d11.dll", LOAD_LIBRARY_SEARCH_SYSTEM32)`.
  This is also a small hardening: a `d3d11.dll` dropped beside the game is
  no longer loaded by accident.
- `vulkan`: DXVK's `dxgi.dll`, then its `d3d11.dll`, by full path from a
  folder of our own beside the engine (`dxvk\x32`, `dxvk\x64`), never from
  the game's folder or the search path.

**The trap in the load.** DXVK's `d3d11.dll` imports `dxgi.dll` by name,
and Windows gives a module that is already loaded under that name. If the
system's `dxgi.dll` is in the process first, DXVK's Direct3D gets
Microsoft's DXGI and the device fails or half-works. Today nothing loads it
first (section 2). After R6, SDL's video start-up does. So the round fixes
the order: DXVK's pair is loaded before SDL's video starts, or SDL is told
not to load DXGI. Stage V0 measures which modules are in the process at the
renderer's start, on both hosts.

**When Vulkan cannot start** (the files are not there, no Vulkan 1.3
driver, the device call fails): the renderer logs why, makes the Direct3D 11
device, and the launcher's next start says so in one line. This is a
defined fall-back, not a silent one: the log line and the notice are part
of it. A switch `BOF3X_RENDER_API=vulkan!` (the harness's) makes the same
failure a `Fatal`, so a proof run cannot pass on the wrong API.

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
3. **The 32-bit game.** If V0 shows DXVK does not fit the hosted game's
   2 GB, Vulkan is offered on the 64-bit build alone, and the box is grey
   with that reason on the 32-bit one. Proposed: accept that, and do not
   set the large-address bit for it.
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

- Every statement about DXVK on Windows: that a current release ships
  32-bit and 64-bit Windows DLLs, that it needs Vulkan 1.3, the files'
  sizes, that it accepts the three swap effects and
  `MakeWindowAssociation`, and that dual-source blending (DIV-0160) and
  the staging readbacks behave as Microsoft's do. The Linux probe proved
  the present pass alone, on DXVK-native, at 64 bits.
- That Windows gives the already-loaded `dxgi.dll` to DXVK's `d3d11.dll`
  (the trap in section 3). It is how the loader is known to treat a name
  already loaded; V0 proves or removes it.
- That `LOAD_LIBRARY_SEARCH_SYSTEM32` is right inside the hosted
  `BOF3.exe` under Wine, where `d3d11.dll` may be an override in the
  prefix. Tier 1 ([`linux-round.md`](linux-round.md) 2) must still draw.
- Frame times. DXVK compiles pipelines at first use; a hitch at a first
  draw of a state is possible and is V4's to measure.
- The in-between frame's presents (DIV-0230) under DXVK's present timing.
- What R6's SDL window and R9's header work leave in the files this round
  edits: the line numbers above are `main`'s at `5683387a` plus R7.
