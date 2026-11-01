# The 64-bit build: the low-memory design

**Status:** DRAFT (2026-11-01, the 64-bit build round's group XA2, written on
`x64/xa2` from the round branch `21d06502` before any source change; every
number below is a probe run on this machine (Windows 11 Pro 10.0.26200, the
`x86_64-w64-mingw32-clang++` driver of llvm-mingw, clang 22.1.8), a count over
the tree, or a file and line - section 14. No game was run. The owner's four
calls of [`x64-build-round.md`](x64-build-round.md) 7 are the inputs: the
narrow image, x86-64 Windows alone with the stack switch shaped for a third
variant, a developer's build beside the 32-bit one, SDL3 for the sound
output.)

[`x64-build-round.md`](x64-build-round.md) 3 asks XA2 for one document, in
the shape of [`relocatable-image-design.md`](relocatable-image-design.md),
that stage B's groups and stage C build to: where the image and every address
the game can see lie in a 64-bit process, and what each of the round doc's
bullets costs, each answered by a measurement. It cites rather than restates:
[`x64-build-round.md`](x64-build-round.md) 0 (what pins the build to 32 bits),
[`relocatable-image-design.md`](relocatable-image-design.md) 1.2 (the 16 MB
window) and 4 (the bind table), [`state3-design.md`](state3-design.md) 5 and
[`quicksave.md`](quicksave.md) 1 and 3 (the task stacks and the quicksave's
span), [`sound-layer-round.md`](sound-layer-round.md) and
[`state-hash.md`](state-hash.md) 3 and 8 (the sound windows).

## 0. The shape in one paragraph

**`bof3x.exe` stays a fixed-base executable at `0x400000`, written by
`make_exe.py` as a PE32+ with no relocations and no `DYNAMIC_BASE`; Windows
11 loads it there (section 1, measured with and without a `.reloc`
section). That one header bit does most of the design's work: with ASLR off
for the process, Windows places the main thread's stack, the toolchain's
heap, every `VirtualAlloc(nullptr)`, the Direct3D 11 device, the 64-bit
`dsound`'s objects and SDL3's stream bottom-up, below 4 GB (section 2: the
stack at `0x60000..0x160000`, the heap from `0x200000`, 44 of 48 64 MB blocks
below 4 GB before the first above). So the game's words stay 32 bits wide
and hold what they hold today: the image's places (the image is the
loader's, as in the 32-bit build), the heap blocks of the 29 allocation sites
(section 5, read one by one), the task slices on the main thread's stack
(section 7), the render shim's and the sound layer's objects (section 6).
`bof3x.dll` keeps its fixed base `0x10000000` and the loader honours it
(section 3), so `Key()` fits 32 bits and the bind table's 5-byte `jmp rel32`
rows reach it under every top byte the table may take (measured, each
called). The 14 import slots get a 14-byte absolute-jump thunk each in a page
of ours below 4 GB (section 4, measured). The task switch is rewritten for
`rsp` with the x86-64 callee-saved set in a shape a third variant slots into
(section 7: the probe's switch preserves a task's registers across yields and
a vectored handler is reached for a fault on a task stack); the slice size is
stage C's stackwatch number, estimated here from the frames of a sample
compiled both ways. The one trap the design asks of stage B is `Low32()`: a
narrowing that is `Fatal` above 4 GB, so that an address this design missed
stops the game by name rather than truncating.** What the design does not
need: a low arena of its own (the OS's placement gives one; a
`Fatal` guards it), a widened `dll_base` in the quicksave, a `jmp` row of
more than 5 bytes, a DirectSound shim (SDL3 is the owner's call; what the
slots hold is our objects either way, section 6).

## 1. The image: a PE32+ module at `0x400000` (option a), measured

### 1.1 The probe

`tools/probes/lowmem_probe.cpp` (re-written 2026-11-01 from the 2026-10-29
three-question version; its header gives the build lines) is one 64-bit
program built three ways and run from one directory with
`tools/probes/lowdll_probe.cpp`'s DLL beside it; the outputs are quoted from
`scratch/lowmem_<variant>.txt` of the XA2 worktree (not in the tree):

| Variant | Link options | What it stands for |
|---|---|---|
| `noreloc` | `--image-base 0x400000 --disable-dynamicbase --disable-reloc-section` | `bof3x.exe` as `make_exe.py` writes it today: no `.reloc`, no `DYNAMIC_BASE` (header: `magic 0x20B machine 0x8664 characteristics 0x23 dllcharacteristics 0x8120 ImageBase 0x400000 .reloc 0 bytes`) |
| `fixed` | `--image-base 0x400000 --disable-dynamicbase` | the same with a `.reloc` section kept (96 bytes), the 32-bit `bof3x.dll`'s choice |
| `aslr` | the driver's defaults | a high executable, option (b)'s host (`dllcharacteristics 0x8160`: `DYNAMIC_BASE` on) |

### 1.2 Does Windows load it at `0x400000`?

**Yes, both ways.** `noreloc`: `module=0000000000400000 peb
ImageBaseAddress=0x400000`; `fixed`: the same. The `aslr` variant lands at
`0x7FF6DDEC0000` (another run `0x7FF6FE460000`, `0x7FF783E00000`): a 64-bit
executable with `DYNAMIC_BASE` is placed high, which is what the 2026-10-29
probe saw when lld's `--image-base 0x400000` alone moved it to `0x4E0000`
(that build still had `DYNAMIC_BASE`; the round doc's section 0 row is
explained by that bit, not by a 64-bit rule).

The PEB reads the same number from `gs:0x60` + `0x10`, which is `ImageBase()`'s
x86-64 spelling (section 8).

### 1.3 What the window below 16 MB holds under each variant

The packets must lie below 16 MB
([`relocatable-image-design.md`](relocatable-image-design.md) 1.2: the
ordering table's links are 24-bit). They live in the image's `.bss` and
`.pool` (`exe_layout.h`: `.game` `0x5C4000` + `0x37B000`, `.pool` `0x93F000` +
`0x48000`, both sections of `bof3x.exe`), so under option (a) **the window
needs to hold nothing but the image**, and the probe's answer for the rest of
it only matters to option (b):

| | `noreloc` / `fixed` (at `0x400000`) | `aslr` (high) |
|---|---|---|
| free in `0x10000..0x1000000` at `main`, after the CRT | `0x1CE000`, `0x13E000`, `0x10E000`, `0xED000` over four runs (largest run `0xF0000` at `0xC00000` at best); the stack `0x60000..0x160000`, the heap from `0x200000`, `locale.nls`, `C_1252.NLS`, `C_437.NLS`, `l_intl.nls` and anonymous section views fill it | `0x9F0000` of `0xFF0000`, one free run of `0x600000` at `0xA00000` |
| `VirtualAlloc(0x400000, 0x600000)` | not asked: the executable is there | succeeds |
| 64 MB at `0x1000000` | **refused, error 487**: an anonymous 16 MB section view sits at `0x1000000..0x2000000` at `main` in every fixed-base run (`GetMappedFileName` names no file) | succeeds |
| after a Direct3D 11 device, a DirectSound device and buffer, SDL3 audio | free `0x117000` (`0xEC000`), largest run `0xF000` | unchanged, `0x9F0000` |

So option (b) - a high executable mapping the image as a block at
`Engine_Main` - would find the window free at `main` (`aslr` row) but must
take it **before** the CRT, D3D11, dsound and SDL3 run, and its `Fatal` would
have to say "the range `0x400000..0x987000` is not free at `Engine_Main`: a
DLL, a heap or a section view took it; start from the launcher, which reserves
it in the child" - the developer launcher's fence exists for the moved base
already (`launcher.cpp FenceLinkedSpan`). It also puts every other object the
game sees above 4 GB (section 2), which is why it loses.

### 1.4 The choice: (a)

| | **(a) `make_exe.py` writes a PE32+ at `0x400000`** (recommended) | (b) a high executable, the image a block |
|---|---|---|
| The image | the loader's, before any heap or thread (as today) | ours, at `Engine_Main`, fenced by the launcher |
| The rest of the process | **below 4 GB without any code of ours** (section 2) | above 4 GB: every pointer the game stores needs an arena of ours |
| `GetModuleHandle(nullptr)` | the image base, as `host.cpp` and `win_main.cpp` expect ([`relocatable-image-design.md`](relocatable-image-design.md) 1.3) | the executable's base, unrelated to the image: a new identity for `DetectHost` |
| `make_exe.py` | the PE32+ header: magic `0x20B`, machine `0x8664`, an 8-byte `ImageBase`, 8-byte stack and heap fields, the data directories at `opt + 112`, `Characteristics` without `32BIT_MACHINE` (`0x100`) and with `LARGE_ADDRESS_AWARE` (`0x20`), the entry stub's five operands as 64-bit code (`call [rip + disp32]` into the IAT; today's `FF 15 abs32` does not exist on x86-64) | the three big sections dropped, a new loader |
| `pe_read.py` | reads `magic` `0x20B`, `ImageBase` as `<Q` at `opt + 24`, `SizeOfStackReserve..SizeOfHeapCommit` as four `<Q` at `opt + 72`, `NumberOfRvaAndSizes` at `opt + 108`; the checks `make_exe.py verify` runs: base `0x400000`, no `.reloc` directory, `DllCharacteristics & 0x40 == 0` (no `DYNAMIC_BASE`) and `& 0x20 == 0` (no `HIGH_ENTROPY_VA`, meaningless without the first but a reader's trap), the section RVAs the layout's | the same for a high file, no base check |
| The quicksave, the crash reporter, the tools | unchanged (sections 10, 11) | the span's base is a run-time block: `proc_image.py`'s "main module base" rule breaks |

**Recommended: (a).** It changes the least and the measurement says the rest
of the process follows the executable's header bit. Section 13 lists what (a)
relies on that stage C must see: that the game's own start-up (the launcher's
child, the window, DirectInput, the MP3 decoder's block) keeps to the same
bottom-up placement the probe saw.

## 2. The process below 4 GB: what `DYNAMIC_BASE` off buys

Measured in the same runs, fixed-base variants against `aslr`:

| What | `noreloc` / `fixed` | `aslr` |
|---|---|---|
| the main thread's stack (TIB `StackBase..StackLimit`, the reservation) | `0x160000..0x15B000`, reserved `0x60000..0x160000` (1 MB, the driver's default; `bof3x.exe`'s header says `0x100000` / `0x10000`, `exe_layout.h`) | `0x914AD50000`: above 4 GB |
| `VirtualAlloc(nullptr)`: the task-stack probe's arena, a 256 KB block (`mp3.cpp`'s decoder stack), a 1 MB fiber's stack (the present's, `render_d3d11.cpp:1009`) | `0x510000`, `0x520000`, `0x96FF74` (`taskstack_fixed.exe`) | `0x14198070000`, `0x141982F0000`, `0x131D8FF844` |
| `malloc(1 MB)`, `malloc(17 MB)` (the toolchain's heap, `Crt_malloc` since crt-rest 2.6) | `0x7ABE040`, `0x7BC6040` (`0x974040`, `0xA81040` in the task-stack probe) | `0x1C25ECDC040`, `0x1C25EDEC040` |
| `malloc(64 MB)` 48 times, 3 GB | **44 below 4 GB**, the first above at `0x100206040` after 2.75 GB | 0 below |
| free below 4 GB at `main`; the largest run | `0xFDF3B000` of `0xFFFF0000`; `0x80011000` at `0x7FFEF000` (the upper 2 GB of the low 4 GB, whole) | `0xFB9EE000`; the same run |
| free below 4 GB after D3D11, dsound, SDL3 | `0xB8715000` (D3D11's driver takes about 1.1 GB of address space) | `0xFB9D5000` (it takes it above) |
| `D3D11CreateDevice`'s device | `0x2C05980` | `0x1C25A4F6BA0` |
| `DirectSoundCreate`'s `IDirectSound`, a secondary `IDirectSoundBuffer` | `0x2EB0C50`, `0x2EBCBA0` (`0x74D0C50`, `0x74DCE60`) | `0x1C25ECC0C50`, `0x1C25ECCC9E0` |
| SDL 3.4.16's playback stream (`SDL_OpenAudioDeviceStream`, the tree's SDL built for x86-64 as a DLL) | `0x6269B0` | `0x1C2584062F0` |
| `lowdll_probe.dll` at its preferred `0x10000000` | honoured | honoured |

Three consequences:

1. **The game-visible class of addresses is below 4 GB by placement, not by
   code of ours.** The OS's bottom-up allocator in a process whose executable
   has no `DYNAMIC_BASE` starts at the bottom; the low 4 GB fills only after
   about 2.75 GB of blocks. The game's peak is far below that (the 32-bit
   build lives in a 2 GB process; the quicksave's largest file is 4.6 MB,
   [`quicksave.md`](quicksave.md) 10.2).
2. **It is a placement, so it is guarded, not assumed.** Stage B2's `Low32(p)`
   (round doc 4) is the trap: every narrowing site `Fatal`s on an address at
   or above 4 GB, naming its file and line. Section 5 adds one chokepoint for
   the heap: `Crt_malloc` and `Crt_calloc` check their result once, so the 20
   `g.malloc` / `Crt_malloc` sites need no site-by-site check.
3. **A low arena of our own is not needed** for correctness; the probe's
   `VirtualAlloc(0x1000000, 64 MB)` fails in every fixed-base run (the 16 MB
   section view at `0x1000000`), and nothing wants that address. If stage C
   ever sees `Low32` fire on a heap block, the fallback is a `HeapCreate`d
   heap over a reserved run below 4 GB (the 2 GB run at `0x7FFEF000`,
   measured free in every run) behind `Crt_malloc`, with `Crt_free` telling
   its blocks by range - one file, `crt_rest.cpp`, no game site.

The **DLL's preferred base must be free when it loads**: in a run where the
probe's 3 GB heap climb preceded `LoadLibrary`, the DLL was moved to
`0xC00000` (the probe's earlier order, 2026-11-01). In `bof3x.exe` the DLL is
loaded by the import table before any code of ours runs, so the heap cannot
be there first; and a relocated DLL still lands below 4 GB (bottom-up), where
`Key()` fits and the quicksave refuses the mismatched `dll_base` as today
(section 10). The `.reloc` section stays on the DLL, as the 32-bit link keeps
it (`CMakeLists.txt`, the `target_link_options` of `bof3x`: "the relocation
section is KEPT").

## 3. Our code: `bof3x.dll` low and fixed, the bind table's rows 5 bytes

- **The loader honours a 64-bit DLL's low preferred base.** `lowdll_probe.dll`
  (`--image-base 0x10000000 --disable-dynamicbase`) loads at `0x10000000` in
  all three variants; `LowDll_Answer` at `0x10001350`.
- **A 5-byte `jmp rel32` row reaches it under every top byte.** The probe
  reserves one page at `(top << 24) | 0x401000`, writes `E9 rel32` to the
  DLL's function and calls through the 32-bit word, for `0x7C`, `0x70`,
  `0x7E`, `0x7B`, `0x7D` (the measured tops, `bind.cpp:72`) and the fallback
  range's ends `0x20` and `0x7F`: every delta is within `-2^31` (`-1,816,132,789`
  for `0x7C`, `-1,866,464,437` for `0x7F`, `-272,628,917` for `0x20`) and every
  call answers `42`. The bound is the farthest row to the DLL's **start**
  (a row jumps down to a function): `0x7F5C4000 - 0x10000000` = `0x6F5C4000`
  = 1.87 GB, inside the 2 GB reach with 0.13 GB to spare, whatever the DLL's
  size (the 32-bit `bof3x.dll`'s `SizeOfImage` is `0x7535000`, 117 MB, read
  from the header of the 2026-10-31 build; a 64-bit one is larger and the
  bound does not move). The **14-byte absolute-jump row is not needed**, and
  the 11-byte nearest gap between rows stays as it is.
- **`Key()` fits 32 bits** (`bind.cpp:22`): every function of ours lies in
  `0x10000000..0x14000000`. It becomes `Low32()` (B2), a `Fatal` if the DLL
  was ever relocated above 4 GB.
- **The table's own reservation** (`Reserve`, `bind.cpp:88`: `VirtualAlloc`
  at `(top | 0x401000) & ~0xFFFF`, `0x1C3000` bytes) is a fixed-address
  request below 4 GB; the probe's `Row()` is that call at page size and
  succeeded under all seven tops in a 64-bit process. The 2 GB run at
  `0x7FFEF000` is above every top byte's range (`0x7F5C4000` < `0x7FFEF000`),
  so the table never competes with the OS's bottom-up placement of the heap
  until the heap has passed 1.9 GB.

What changes in `bind.cpp` for the width: `Key()` through `Low32()`; the
`BuildTable` casts `static_cast<std::uintptr_t>(from)` already; the
`DirectSoundThunk`'s `CreateFn` is `__stdcall` on x86-64 only in spelling
(ignored), and under call 4 it binds to the SDL3-backed `Create` of section
6. `Bind_VaOfFunction`'s `by_ours` sorts 32-bit keys: unchanged.

## 4. The import slots: a thunk each, 14 bytes, below 4 GB

The 14 slots (`exe_host.cpp` `kSlots`) hold a system DLL's function, which in
a 64-bit process lies at `0x7FFA...` (`GetTickCount` at `0x7FFA95F98600`,
`dsound.dll` at `0x7FFA1B9F0000`: above 4 GB in every variant). Measured:
**a 14-byte thunk `FF 25 00 00 00 00` + the 8-byte target (`jmp [rip+0]`) in
a page of ours below 4 GB, called through a 32-bit word, reaches
`GetTickCount`** (`import-slot thunk at 0x00C00000 -> GetTickCount
00007FFA95F98600: 1483391078 in 1483391078..1483391078 (ok)`: the answer
between a `GetTickCount` before and after, the `BindSlots` self-test's own
check, `exe_host.cpp:447-451`).

The design:

- **Where:** one page of the bind table's reservation, `(top << 24) | 0x5C4000`,
  the page right after the table's last row page (`kSpanHi` `0x5C4000`;
  `Reserve` grows by `0x1000`), so one reservation holds every place a
  32-bit code word of ours can name, and `Bind_TableLo..Hi` still bounds the
  table proper. The thunks are laid 16 bytes apart in slot order: slot *i*'s
  place is `(top << 24) | 0x5C4000 + 16 i`. Not at `(top << 24) | slot VA`:
  three slots are 4 bytes apart (`0x5C4020`, `0x5C4024`, `0x5C4028`;
  `0x5C4080..0x5C4088`), which a 14-byte row cannot fit, and a slot word is
  not a code word the state hash maps (`.rdata` `0x5C4000..0x5DA000` lies
  below the hash's `0x5DA000`, `statehash.cpp:129`).
- **`BindSlots`** writes the thunk (`FF 25 00000000` + `FARPROC`) and stores
  the thunk's place as the 4-byte slot word, in place of `memcpy(Place(s.at),
  &fn, 4)` (`exe_host.cpp:437`), which on x86-64 would store half a pointer.
  The read-back self-test compares the slot word with the thunk's place and
  calls `GetTickCount` through `Imp_GetTickCount` as today.
- **The readers need one change:** `Imp_GetTickCount` (`game_clock.cpp:90-95`,
  DIV-0022's `PatchBytes` replaces the slot word with `Low32(&SessionTickCount)`,
  our function in the DLL, below 4 GB) computes its `expected` bytes from
  `&GetTickCount`, which on the 64-bit host is not what the slot holds: it
  takes the slot's current word (the thunk's place) instead, in both widths
  (hosted the loader's `GetTickCount`, which it is today). The rest need
  nothing: `gfx_texcache.cpp:213`'s "bound yet"
  test (non-zero), `shell.cpp:49`, the callee headers that call through the
  slot, `wav_dump.cpp`'s hook of `0x5C4014` (a function of ours in the word),
  the self-test. A thunk is a code place that calls like the function it
  stands for, which is all a slot's reader asks.
- **`0x5C4014` (`DirectSoundCreate`)** is the one slot whose function the
  design replaces rather than thunks: under call 4 the word holds the SDL3
  sound layer's `Create` (section 6), a function of ours, and the bind
  table's row for the thunk `0x5ACBBC` jumps to `DirectSoundThunk` as today,
  which calls through the slot (`bind.cpp:37-45`).

## 5. The heap the game sees: the 29 live allocation sites, read

Every live site (the round doc's 29: `grep -rnE "(^|[^A-Za-z_])(malloc|Crt_malloc|VirtualAlloc|calloc|Crt_calloc)\("
src/game`, fuzz files apart), with where its address goes:

| Site | Call | Where the address goes | Class |
|---|---|---|---|
| `crt_rest.cpp:335`, `:336` | `Crt_malloc` / `Crt_calloc` = `std::malloc` / `std::calloc` | the allocator the game's callee tables name (`display_env_callees.h:77`, `save_menu_callees.h:153`, `sound_callees.h:80`: `Crt_malloc`); its callers below | **the chokepoint** |
| `dat_cache.cpp:98` (`CrtMalloc`), `:348` (`ReadWhole`) | `g_c.io.malloc` | a cache file read whole, walked by `WalkDatFile`'s taker and freed (`dat_load.cpp:348`); its chunks are copied into the arena (`Arena() + tag`, in `.bss`) | ours, transient |
| `dat_load.cpp:337` | `g_io.malloc(file_size)` | the DAT file read whole, freed at `:348` after the walk | ours, transient |
| `dat_load.cpp:453` | `std::malloc(size + pad)` | `s->data`, the reading row's side chunk, a DLL-side table (DIV-0147's family; never in game memory) | ours |
| `dat_load.cpp:482` | `Crt_malloc(h.size)` | the glyph table, owned by `Font_SetGlyphData` -> **`Font_GlyphData` `0x7CC35C`** (`gfx_image.cpp:45`; the quicksave's keep list) | **game** |
| `display_env.cpp:262` | `g.malloc(bytes)` | a bank's sample data -> the bank record `+0x380` (**`Sound_Banks` `0x6BC928`**), every voice entry rebased onto it (`:270-272`), DirectSound (SDL3 then) reads from it | **game** |
| `draw_pool.cpp:280` | `VirtualAlloc(want, 0x48000)` below 16 MB | the draw items (the hosted fallback; `bof3x.exe` uses `.pool`) | **game** (hosted only) |
| `gfx_clut.cpp:64` | `Crt_malloc(0x1000)` | `row.pixels` of **`Gfx_ClutRows` `0x6C2A40`** (the keep list: "row pointers") | **game** |
| `map_view.cpp:92` | `std::malloc(2 * 0x820000)` | the `AreaMap_ByteAt` shadow self-test's 17 MB plane, hosted only, given back | ours (hosted) |
| `music_seq.cpp:257` | `g.malloc(size)` | **`Music_File` `0x6BDE50`** (DIV-0087's song file) | **game** |
| `music_seq.cpp:641` | `TestMalloc` | the self-test's stand-in | ours |
| `save_menu.cpp:209` | `g.malloc(size)` | the DAT read whole and walked, freed at `:224` | ours, transient |
| `save_menu.cpp:445` | `g.malloc(size)` | **`SaveMenu_StreamData` `0x6BDE44`** | **game** |
| `sound.cpp:210` | `g.malloc(size)` | **`Music_File`** (`Music_LoadFile`) | **game** |
| `sound.cpp:804` | `g.malloc(size)` | **`Music_Data` `0x7DE3D4`** (`Music_Start`'s copy) | **game** |
| `sound.cpp:1844`, `:1885` | `g.malloc(bytes)` | the quicksave's SOUND restore: a bank's data -> `Sound_Banks`, the last stream's file -> `SaveMenu_StreamData` | **game** |
| `sound.cpp:2089`, `:2338`, `:2425`, `:2437`, `:2458` | `TestMalloc`, `std::malloc` | the SOUND self-test's stand-ins (hosted `BOF3X_SHADOW=sound` and the exe-host self-test) | ours |
| `text_advance.cpp:92` | `Crt_malloc(count)` | `g_advances`, a DLL static (DIV-0006) | ours |
| `text_pairs.cpp:25` | `Crt_malloc(4 count + 4)` | `g_pairs`, a DLL static (DIV-0057) | ours |

Counted: **9 sites hand an address to the game** (a `GameData` slot or a
record: `Font_GlyphData`, `Sound_Banks` twice, `Gfx_ClutRows`, `Music_File`
twice, `SaveMenu_StreamData` twice, `Music_Data`; plus the hosted draw pool),
**4 are transient file buffers** the game never keeps, **the rest are DLL
statics and self-test stand-ins**. Every game-visible block comes through
`Crt_malloc` (the callee tables' `malloc`) or `std::malloc` in a file that
also uses `Crt_free` on it - the one heap (crt-rest 2.6: `malloc` /
`calloc` / `free` bound as a set). So:

- **One chokepoint:** `Crt_malloc` and `Crt_calloc` (`crt_rest.cpp:335-336`)
  return `Low32`-checked blocks - a `Fatal` naming the size if a block lies
  at or above 4 GB. The 9 game sites then need no edit; the `std::malloc`
  sites that reach a slot (`sound.cpp:804` writes `Music_Data` through
  `g.malloc`, which is `Crt_malloc` live) are the same chokepoint. The
  transient and DLL-static sites are free to be anywhere; `Low32` at a
  narrowing site catches a transient buffer whose address is stored
  (`save_menu.cpp:215`'s `Addr(data)`: a `U` computed from the block and
  used while it lives - checked by `Addr`, which is a narrowing site of the
  384).
- **The quicksave's `block_bytes`** (`sound.cpp:1608`, `_msize`) stays the
  toolchain's heap's, since the heap is the toolchain's.
- **No arena, no second allocator** (section 2, consequence 3 holds the
  fallback).

## 6. The objects the game keeps a pointer to

### 6.1 The render shim's

`render_shim.cpp:105-111`: `g_dd`, `g_device`, `g_viewport`, `g_material` are
static singletons in the DLL's data, each a vtable pointer; surfaces are
`Surface` objects of ours. In the DLL at `0x10000000` every one is below
4 GB: their addresses fit the slots (`D3d_Device` `0x7CC350`, `Dd_DirectDraw`
`0x7CC334`, the keep list's rows) as they do today. `Method<>` in the three
callee headers reads the vtable as `void* const*` and indexes `offset / 4`
(`sound_callees.h:71-75`): on x86-64 that is an index into 8-byte entries,
right for a vtable of ours, so the 54 sound and the display sites that call
through it need no change for an object of ours. Nothing moves.

### 6.2 The sound objects: what the 32-bit slots hold today

The game keeps Windows' DirectSound objects in 32-bit slots of the image
(`symbols.toml`): `Snd_Device` `0x7DE3BC`, `Snd_Primary` `0x7DE3C0`,
`Music_Buffer` `0x7DE3C4`, `SndStream_Buffer` `0x7DE3C8`, `Music_Notify`
`0x7DE3CC`, `Music_Events[2]` `0x7DE3B4` (two event handles), and per bank
64 voice entries' buffers (`Sound_Banks` `0x6BC928`, 5,400 bytes; the entry
`+4` written at `display_env.cpp:272` from `g.from_wave`) and the 23
`Sound_Channels` `0x6BC8C8` words (a looping voice's buffer). `Music_Decoder`
`0x7DE3D0` and `Music_Data` `0x7DE3D4` are ours already (DIV-0091's decoder,
a heap block). The game calls the objects through **54 `Method<>` sites in 5
files** (`sound.cpp` 32, `sound_rest.cpp` 9, `battle_items.cpp` 7, `sound_sp.cpp`
5, `sound_callees.h` 1; `grep -n "Method<" src/game/{sound,sound_rest,battle_items,sound_sp}.cpp src/game/sound_callees.h`),
on **21 distinct methods of three interfaces** (the vtable offsets in
`sound_callees.h:50-56`, `sound_rest_callees.h:32-33`, `sound_sp.cpp:47`, `sound.cpp:1577`,
`battle_items.cpp:762-786`):

| Interface | Methods the game calls |
|---|---|
| `IDirectSound` | `CreateSoundBuffer` (`0x0C`), `SetCooperativeLevel` (`0x18`), `Release` |
| `IDirectSoundBuffer` | `QueryInterface` (`0x00`, for the notify), `Release` (`0x08`), `GetCurrentPosition` (`0x10`), `GetVolume` (`0x18`), `GetPan` (`0x1C`), `GetFrequency` (`0x20`), `GetStatus` (`0x24`), `Lock` (`0x2C`), `Play` (`0x30`), `SetCurrentPosition` (`0x34`), `SetFormat` (`0x38`, the primary), `SetVolume` (`0x3C`), `SetPan` (`0x40`), `SetFrequency` (`0x44`), `Stop` (`0x48`), `Unlock` (`0x4C`), `Restore` (`0x50`) |
| `IDirectSoundNotify` | `SetNotificationPositions` (`0x0C`), `Release` |

The descriptors the game builds are the 32-bit ones: `Snd_BufferDesc`
`0x7DE378` is a `DSBUFFERDESC` of `0x24` bytes with `lpwfxFormat` at `+0x10`
holding `Img(0x7DE3A0)` (`sound_rest.cpp:216-221`, `sound.cpp:500-506`), the
`WAVEFORMATEX` `Snd_WaveFormat` `0x7DE3A0` of `0x12`. **The x64 `DSBUFFERDESC`
is 40 bytes**, an 8-byte `lpwfxFormat` at `+0x10` and the GUID at `+0x18`
(`sizeof(DSBUFFERDESC) 40 (the game's 0x24)` in the probe): a 64-bit `dsound` behind the slots would need every
descriptor translated, every object pointer wrapped, and a `GetCurrentPosition`
that takes 64-bit out-pointers while the game passes 32-bit places. The 64-bit
`dsound`'s objects did land below 4 GB in the fixed-base runs (`0x2EB0C50`),
so the slots could hold them - but the vtables are x64 vtables, which
`Method<>` indexes right, while every argument that is a pointer (`Lock`'s
four out-pointers, `GetStatus`'s, `GetCurrentPosition`'s two) is passed by the
game as a 32-bit place: 11 of the 21 methods take one. That is the DirectSound
shim's cost: 21 methods, each marshalling, in front of an API the owner's
call 4 retires.

### 6.3 What an SDL3-backed sound layer puts in the slots (call 4)

**Objects of ours with DirectSound-shaped vtables, as `render_shim` is in
front of Direct3D 11.** The shape:

- `Create` (bound to the `DirectSoundCreate` slot and the `0x5ACBBC` row):
  writes `&g_device` (a static of ours, in the DLL) into `*out` as a 32-bit
  word (the out-pointer is `&Snd_Device`, a 32-bit slot: section 9's
  `Slot32<T>` makes `&Snd_Device` a `Slot32<void>*`, which `Create` takes).
- The device's `CreateSoundBuffer(desc, out, outer)`: reads the game's 32-bit
  `DSBUFFERDESC` (size `0x24`, flags, bytes, the format at `+0x10` as a
  place), makes a `Buffer` of ours (a PCM ring in a heap block, the format,
  volume, pan, frequency, play cursor, status, loop flag) and writes its
  address into the 32-bit out-slot. The primary buffer is a `Buffer` with
  `SetFormat` and no data.
- The buffer's 19 methods, DirectSound semantics on our ring: `Lock` /
  `Unlock` hand out the ring's bytes (the game writes with `SndBuf_Write`);
  `Play(flags)` with `DSBPLAY_LOOPING`; `Stop`; `GetStatus` (`DSBSTATUS_PLAYING`,
  `LOOPING`; never `BUFFERLOST`, so `Restore` is a no-op and
  `battle_items.cpp:764`'s lost-buffer path is dead); `SetCurrentPosition` /
  `GetCurrentPosition` (the play cursor in bytes, advanced by what the mixer
  consumed; the write cursor as DirectSound's lead); `SetFrequency` /
  `GetFrequency` (the resample ratio, SDL3's
  `SDL_SetAudioStreamFrequencyRatio`); `SetVolume` / `GetVolume` (hundredths
  of dB, DirectSound's law, kept as the game's number and applied as a gain;
  DIV-0171 and DIV-0172 compute in that law); `SetPan` / `GetPan` (the
  same, applied as a stereo gain pair before the stream); `QueryInterface`
  for the notify object, whose `SetNotificationPositions` records the
  positions and the game's event handles (`Music_Events`, made by
  `CreateEventA` through its slot) which the mixer signals when the play
  cursor crosses one - `Music_Pump`'s wait on them through
  `MsgWaitForMultipleObjects`'s slot stays as it is.
- **The output:** one SDL3 device stream (`SDL_OpenAudioDeviceStream`, 44,100
  Hz 16-bit stereo, the primary's format) fed by a mixer of ours on SDL3's
  callback (or a put each frame from `Sound_Tick`), summing every playing
  `Buffer` at its frequency, volume and pan. Not one SDL3 stream per buffer:
  the play-cursor semantics (position in bytes, notifications at positions,
  `GetCurrentPosition` while playing) are ours to compute, and a mixer
  computes them exactly; SDL3's per-stream resampler would hide the cursor.
- **The slots are written by the game's own code as today** (`sound.cpp:839`
  `Music_Buffer = buffer`, `display_env.cpp:272`): the values are our objects'
  addresses, below 4 GB.

Sized against the alternatives: a C++ interface in place of the vtables edits
the 54 sites, the three callee tables (`sound_callees.h:110` `sndbuf_create`,
`sound_rest_callees.h:49` `create`, `display_env_callees.h:80` `from_wave`)
and the SOUND participant's self-test stand-ins (`sound.cpp:2016-2118`,
`FakeBuf` with a vtable of ours); the vtable shape edits none of them and
is the same shape the self-test already fakes. The mixer is the new code
either way (DirectSound's software mixer did it for the 32-bit build). So the
SDL3 layer is **one new file** (`src/audio/sound_sdl.cpp`: the three objects,
the mixer, the SDL3 stream) plus the slot binding in `exe_host.cpp`
(section 4), and B5's work in the game files is zero outside the `Slot32`
spelling of section 9.

### 6.4 What B5 must keep identical for the state hash's sound windows

The hash reads `.data` `0x5DA000..0x93E000` ([`state-hash.md`](state-hash.md)
3; `statehash.cpp:129-130`), and its skip list takes the sound objects out
already (`tools/statehash_skip.txt:25-32`, `:206`): `Sound_Banks` (5,400,
"buffer pointers in every record"), `Sound_Channels` (92), `SaveMenu_StreamData`
(`0x6BDE44`), `Music_File`, `Music_Staging` (`0x7CC378`, 73,728), `Music_Events`
.. `Music_Data` (`0x7DE3B4`, 40), `Music_Finished` (`0x7DE3E0`), each
"noise": run-to-run pointers and the stream's timing. The "sound windows" of
the references are the scattered ticks on pages `0x7DE000` and `0x6BC000`
where those words change ([`state-hash.md`](state-hash.md) 4's third row,
5 item 1, 8's `orig4` note), the noise class the proofs already read past.

**What is hashed and sound's**, which the game's code writes and B5's objects
must leave as DirectSound left them:

| Word | Who writes it | What B5 must give |
|---|---|---|
| `Music_Volume` `0x6BDE5C`, `Music_FadeCount` / `Music_FadeStep` (DIV-0028, skip rows `:191-192`), `Music_Track`, `Music_LoadedTrack`, `Music_FileSize`, `Music_FileLoops`, `Music_Loops`, `SaveMenu_StreamKind` | the game's own functions, ours since the sound rounds | nothing: no object answers into them |
| `Snd_BufferDesc` .. `Snd_WaveFormat` `0x7DE378` (`0x3C` bytes, page `0x7DE000`) | `SndBuf_CreateFromWave`, `Music_CreateBuffer`, `Snd_Init` before each create ([`quicksave.md`](quicksave.md) 10.2 item 7) | **read, never written**: the shim reads the 32-bit descriptor; DirectSound did not write it either, and a changed byte there would be a new page in every route |
| the words the game fills from an object's answer: `GetStatus` -> `Sound_StreamDone`'s result and the inn's / camp's waits (`sound_rest.cpp:141`, `:170`), `GetCurrentPosition` -> DIV-0092's heard sample (`sound.cpp:1093`, `:1679`, `:1698`), `GetFrequency` -> the quicksave's channel record | our objects | the **same answers at the same frames**: a status that reads `PLAYING` from `Play` until `Stop` or a once-only buffer's end, a cursor that advances at the buffer's rate - the class that is already noise between two runs of one build (the stream's own timing), so the proof's tolerance is the references' |
| `Sound_Channels`' entry words (not the buffer word) and `Sound_Banks`' cues | the game, from the bank payload | nothing |

So the **sound windows are the same noise class** on the 64-bit host; the
new requirement is only that no page outside them moves: `Snd_BufferDesc`'s
`0x3C` bytes untouched by the shim, and `Music_Finished`'s timing within the
window the references show (12 frames, `statehash_skip.txt:32`). The WAV dump
(`tools/wav_dump.py`, the sound round's gate) hooks `DirectSoundCreate`'s slot
to wrap the device (`wav_dump.cpp:16`, `:563-578`): under the SDL3 layer it
wraps `Create`'s device the same way, or taps the mixer's output - B5's
choice, and the dump is how the owner's listen is proved
([`sound-layer-round.md`](sound-layer-round.md) 5).

## 7. The task stacks and the switch

### 7.1 Where the slices lie: on the main thread's stack, as today

`Task_SetStackBase` sets `Task_StackTop` to the arriving `esp - 0x4000` on
the main thread's stack, and slot *k*'s slice is the `0x4000` bytes below
`Task_StackTop - k * 0x4000` (`task_sched.cpp:132-165`, `:257-266`;
[`quicksave.md`](quicksave.md) 1). The 32-bit `bof3x.exe`'s stack is at
`0xB0000..0xF0000` (crt-startup 11.4). Under option (a) the 64-bit
process's main stack is **`0x60000..0x160000`** (section 2: TIB
`StackBase` `0x160000`, the 1 MB reservation `0x60000 + 0xF8000` private at
`main`; the header's reserve is `make_exe.py`'s to set, `exe_layout.h`
`kStackReserve`), so `Task_StackTop` and the records' saved stack pointers
are 32-bit values **as they are**, the quicksave's task arena rule
("`[Task_StackTop - n, Task_StackTop)`", [`quicksave.md`](quicksave.md) 3)
holds, and the stackwatch's slices are where it looks. An arena of ours below
4 GB (`VirtualAlloc(nullptr)` gave `0x510000` in the fixed-base probe) is
the fallback if stage C's `Task_SetStackBase` ever reads an `rsp` at or above
4 GB (`Low32` there is the trap); it costs the quicksave a stable
`Task_StackTop` between runs, which the main stack gives for free.

### 7.2 The switch for `rsp`: the probe's shape, measured

`tools/probes/taskstack_probe.cpp` runs the contract of `task_sched.cpp` with
`rsp`: a task's saved stack pointer a 32-bit word in a record of the
original's shape (`+0` state, `+4` saved sp, `0x20` a record), the scheduler's
sp a 32-bit word, four slices of `0x4000`, `Task_RunAll` / `Task_BackToScheduler`
/ `Task_Yield` / `Task_Exit` as naked functions. The callee-saved set the
Win64 ABI gives a C++ task body across a call is **`rbx rbp rsi rdi r12..r15`
and `xmm6..xmm15`** - 8 x 8 + 10 x 16 = **224 bytes** under the return
address (the 32-bit set is 6 x 4 = 24). Measured on `taskstack_fixed.exe`
(the same link options as `noreloc`):

- **registers survive a yield:** task A runs a loop of six doubles and four
  integers that yields 50 times while task B overwrites every callee-saved
  register and `xmm6..xmm15` between the yields; the result equals the
  straight run to the bit (`doubles preserved (2012.5289978091544 vs
  2012.5289978091544), integers preserved`, 61 walks);
- **the entry frame:** the first sp is `kFirstFrame` = `0x110` below the
  slice's top and the entry address at sp + `0xE0` (`kEntrySlot`, above the
  224-byte set), which gives `rsp` = top - `0x48` at the entry, `rsp mod 16 =
  8` as a called function expects, and the 32 bytes of home space Win64 lets
  a callee write above its return address inside the slice (`the home space
  above it inside the slice: yes`). The 32-bit constants are `0x1C` and
  `0x18` (`task_sched_callees.h:28-30` `kStackSize`, `kFirstFrame`, `kEntrySlot`);
- **a fault on a task slice reaches the vectored handler** (the crash
  reporter is one, `crash.cpp:302`): with `rsp` at `0x517FD8`, outside the
  TIB's `StackLimit..StackBase` `0x15D000..0x160000`, the handler ran once,
  rewrote `Rip` and the task went on to exit. (On the main-thread stack of
  7.1 the slices are inside the TIB's bounds anyway; the measurement covers
  the arena fallback.)
- `taskstack_aslr.exe` stops at once: its arena lies at `0x14198070000`,
  above 4 GB - the 32-bit saved-sp words cannot hold a high stack, which is
  section 1's choice again.

The probe's `Task_RunAll64` is the x86-64 text B1 starts from: the eight
pushes, `subq $160, %rsp` and ten `movups` of `xmm6..xmm15` (unaligned
stores: after the pushes `rsp` is 8 mod 16), `movl %esp, Task_SchedulerEsp`
(a 32-bit store of a below-4 GB `rsp`), the walk by `edx` as the original's,
`movl 4(%rbx,%rdx), %esp` to enter (a 32-bit load zero-extends into `rsp`),
the ten loads, `addq $160, %rsp`, eight pops, `ret`. `Task_Sleep` pushes the
same set under its return address and stores `esp`; `Task_Restart` and
`Task_Create` build the first frame with `kFirstFrame64` / `kEntrySlot64`.
`eax`'s contract (left as the previous yield left it) is `rax`'s, and `edx`
= the record offset at the landing as today; `Bof3_ImageMove`'s successor is
the image's 64-bit base in a register loaded RIP-relative
(`leaq Bof3_ImageBase(%rip)` or a `movq` of a 64-bit variable), the VA the
displacement, as the C1 shape has it.

### 7.3 The x87 and MXCSR state: nothing per task, measured

The Win64 ABI treats the x87 control word and MXCSR as callee-preserved, as
the i686 one does; the game sets the control word once (`SetX87`, `0x027F`,
`exe_host.cpp`). A control-word asm block that held a call could yield with a
changed word: **none does** - of the 38 live asm blocks that touch `fnstcw`,
`fldcw`, `stmxcsr` or `ldmxcsr` (section 14's script; the round doc's 23
are the ones with nothing else), **0 hold a `call`**, so every change is
restored before the block ends. The x87 stack is volatile across any call.
So the 64-bit switch saves no floating-point control state, as the 32-bit
one saves none: the set above is complete.

### 7.4 The slice size: stage C's number, estimated here

The 32-bit need is `0x14294` below base for the four slices and the frames
above them (crt-startup 11.4), with slice 0 using `0x2B14` of its `0x4000`
on the F9 route. **64-bit frames are larger**: `tools/probes/frame_growth.py`
compiles every asm-free live file of `src/game` with both drivers (`-S -O2
-g -std=c++20`, the build's flags and include dirs) and reads each
function's prologue (pushes plus the largest `sub $N, %esp` / `%rsp`),
matched by mangled name. **Measured 2026-11-01 (`--all --gen64`, output
`scratch/frame_growth2.txt` and `frame_growth.tsv` of the XA2 worktree): 62
files compile both ways, 211 are skipped (their x86-64 compile fails at a
pointer-typed slot used as a pointer, section 9 - the measure of B3's reach
as much as a limit of the sample), 722 functions matched with a frame. The
sum of the 32-bit frames is 31,320 bytes, of the 64-bit 68,232: ratio
2.18.** By shape: the 18 functions with a 32-bit frame of 256 bytes or more
grow from 16,200 to 18,976 bytes (1.17: locals dominate, and an `int` is
still 4 bytes), while the small frames - most of a call chain - grow by the
pushes (8 bytes a register in place of 4, two to four more callee-saved
registers in use: median ratio 4.5 on frames of 8..32 bytes). The largest
64-bit frame in the sample is 3,768 bytes (`shoulder_glyphs::Arm`, 3,528 on
i686); none of the sample's functions is a task body. So a task's depth, the
sum of a chain of mostly small frames, is expected to about double: slice 0's
measured `0x2B14` of `0x4000` on the F9 route (crt-startup 11.4) becomes
about `0x5600`, past the `0x4000` slice, and the estimate for `kSlice64` is
**`0x8000`** (four slices `0x20000`, the arena copy's "at most" constant
with it) - to be set, not confirmed, by stage C's stackwatch.

The slice size for the 64-bit build is therefore a constant of the x86-64
switch (`kSlice64`, with `kStackReserve` / `kStackCommit` in the PE32+
header raised with it: `0x100000` / `0x40000` holds four slices of `0x8000`
with the 32-bit margin), to be set from stage C's `BOF3X_STACKWATCH=1` run
on the routes - the only measurement that sees the real deepest frame. The
quicksave's arena copy is "at most `0x10000`" today (`quicksave.md` 3), a
constant that follows `kSlice`.

### 7.5 The shape a third variant joins (the owner's call 2)

`task_sched.cpp` keeps the C++ (`Task_Create`, `Task_ClearPrivate`, the
inject list) and the record and word addresses; the five naked functions
and the two frame constants come from **one header per architecture**,
selected by the compiler's target macro and refused otherwise:

```
// src/game/task_switch.h
#if defined(__i386__)
#include "game/task_switch_i686.h"     // Capcom's instructions, esp, the six-register set; kFirstFrame 0x1C, kEntrySlot 0x18
#elif defined(__x86_64__)
#include "game/task_switch_x86_64.h"   // rsp, the eight GPRs and xmm6..xmm15; kFirstFrame 0x110, kEntrySlot 0xE0
#else
#error "task_switch: no switch for this architecture (an ARM64 variant: x19..x28, fp, lr, d8..d15 on sp; docs/x64-build-design.md 7.5)"
#endif
```

Each header defines the same five symbols with the same contract, stated
once in `task_switch.h`'s comment: (1) a task's saved stack pointer is a
32-bit word at record `+4`, the scheduler's at `Task_SchedulerEsp`; (2)
entering a task is "load its sp, pop the architecture's callee-saved set,
return into it"; (3) a yield pushes that set under its return address, stores
its sp and jumps to the landing with the record offset in the walk's
register; (4) `Task_Create` and `Task_Restart` build a first frame of
`kFirstFrame` bytes with the entry at `kEntrySlot`, the home space and
alignment the ABI wants above it; (5) the x87 / SIMD control state is not
saved (7.3). An ARM64 variant is the third file: `x19..x28`, `x29` (`fp`),
`x30` (`lr`) and `d8..d15` on `sp`, `ret` through `x30` in place of the `ret`
that pops - the same five functions, the same words, and `stackwatch.cpp`'s
`kSlice` per variant. This is also where the round doc's rule 2.2 ("a game
file that needs `#if` by width is a defect") makes its one exception,
named: the switch is the exception by construction.

### 7.6 The other stacks

The present's fiber (`render_d3d11.cpp:1009`, `CreateFiberEx(256 KB, 1 MB)`)
and the quicksave's (`snapshot.cpp:669`) are the system's: a fiber's stack
is placed bottom-up like a thread's (`0x96FF74` in the fixed-base probe) and
its switch saves the full context itself (`FIBER_FLAG_FLOAT_SWITCH`). Nothing
of the game stores a fiber-stack address. `mp3.cpp`'s decoder stack
(`VirtualAlloc(nullptr, 256 KB)`, `:167`) is switched by asm (`movl %%esp,
%%esi` ... `:178-188`): B1 rewrites the block for `rsp` (the call's two
arguments in `rcx` / `rdx`, the 32 bytes of home space under the call, the
block's top aligned to 16 - 8), and the block's address needs no width: its
top is in a register, stored nowhere the game reads.

## 8. `ImageBase()`, `Img()`, `VaOf()` by width

### 8.1 What the measurement says about the native-pointer `Img()`

The round doc's table 1 has "`Img(va)` returns a native pointer; `VaOf()`
narrows with a check". Counted (`grep -rhoE "\bImg\(" src | wc -l`): **14,977
`Img()` places**, feeding accessors and `U` (`std::uint32_t`) parameters
everywhere; **950 widenings** (`reinterpret_cast<T*>(static_cast<std::uintptr_t>(u32))`)
and **384 narrowings**. If `Img()` returned `std::uintptr_t`, every one of
the 14,977 sites that passes it to a `U` would narrow implicitly and silently
(the compiler warns only under `-Wconversion`, which the build does not
use), and nothing would be checked. If `Img()` keeps returning
`std::uint32_t` - a place below 4 GB, which under this design every image
place is (`0x400000..0x987000`) - the 14,977 sites compile as they are, the
950 widenings zero-extend a correct value, and the check lives in one
function, `ImageBase()`, read from the PEB.

**This design keeps `Img()` 32 bits wide** and makes `ImageBase()` width-aware,
with the narrowing checked once at start (`DetectHost`) and, before that,
true by construction (a PE32+ with no `.reloc` cannot be relocated: the
loader either maps it at `0x400000` or fails the process, section 1.2). The
proposed header text, replacing `image_base.h:60-73`:

```cpp
// Where the image is (design 1.4; x64-build-design.md 8): the main module's
// base, read from the process environment block - x86: fs:0x30 is the PEB,
// ImageBaseAddress at +8; x86-64: gs:0x60 is the PEB, ImageBaseAddress at
// +0x10. A 32-bit value in both: the image lies below 16 MB (host.cpp) and
// the 64-bit bof3x.exe is a fixed-base PE32+ the loader maps at 0x400000 or
// not at all (no .reloc, no DYNAMIC_BASE; x64-build-design.md 1), so the
// narrowing below loses nothing; DetectHost checks the PEB's value against
// GetModuleHandle(nullptr) and the window before the game starts.
[[gnu::always_inline]] inline std::uint32_t ImageBase() {
#if defined(__i386__)
    std::uint32_t base;
    asm("movl %%fs:0x30, %0\n\t"
        "movl 8(%0), %0"
        : "=r"(base));
    return base;
#elif defined(__x86_64__)
    std::uintptr_t peb;
    asm("movq %%gs:0x60, %0" : "=r"(peb));
    std::uintptr_t base;
    asm("movq 0x10(%1), %0" : "=r"(base) : "r"(peb));
    return static_cast<std::uint32_t>(base);   // checked by DetectHost (host.cpp)
#else
#error "ImageBase: no PEB read for this architecture"
#endif
}

// A VA in the image's data, as a place: a 32-bit value on every host, since
// the image lies below 16 MB (design 1.2). The memory accessors widen it
// (zero-extend) where they dereference.
[[gnu::always_inline]] inline std::uint32_t Img(std::uint32_t va) { return ImageBase() + (va - image::kBase); }

// A place in the image as its VA, for a message. The place is 32 bits wide
// by the same rule; a native pointer that may lie anywhere is narrowed by
// Low32() (common/low32.h), which is a Fatal at or above 4 GB.
[[gnu::always_inline]] inline std::uint32_t VaOf(std::uint32_t place) { return place - ImageBase() + image::kBase; }
```

and beside it, B2's helper, the one checked narrowing of the design
(`src/common/low32.h`, free of Windows so the generated header may include
it):

```cpp
// A native address as the 32-bit word the game's memory holds (x64-build-
// design.md 2): on the 32-bit build the cast; on the 64-bit build a Fatal
// when the address lies at or above 4 GB - the loud trap for an address
// the low-memory design missed, named by the caller's file and line.
[[gnu::always_inline]] inline std::uint32_t Low32(const void* p, const char* what = __builtin_FUNCTION()) {
    const auto v = reinterpret_cast<std::uintptr_t>(p);
#if UINTPTR_MAX > 0xFFFFFFFFu
    if (v > 0xFFFFFFFFu) Fatal("%s: %p lies above 4 GB, where no word of the game can hold it", what, p);
#endif
    return static_cast<std::uint32_t>(v);
}
```

(`Fatal` is declared in `hook/log.h`; the header forward-declares it.) The
PEB read's `gs:0x60` is measured: the probe's `PebImageBase()` is that asm
and printed `0x400000` beside `GetModuleHandleW(nullptr)` in every run.

### 8.2 What this costs the 32-bit build

Nothing: the `#if defined(__i386__)` branch is today's text byte for byte,
and `Low32()` on the 32-bit build is the cast (`0 differ` under
`codegen_same.py`, the gate B2 runs). The `fs:` reads in `boss_harness.cpp`
and `scenario_harness.cpp` (the TIB's stack base and limit) are hosted-only
code, outside the exe host, and get the same `#if`.

## 9. The generated pointer-typed slots: `Slot32<T>` (for B3 and XA1)

A measurement the frame-growth probe forced: compiling any game file with
the x86-64 driver against today's `build/gen/bof3/symbols.gen.h` fails at
`static_assert(sizeof(bof3::GameData) == 0x37B000)` and the `_addr`
asserts after it, because **111 `[[data]]` entries are pointer-typed**
(`tomllib` over `symbols.toml`: 73 `void *`, 28 `unsigned char *`, 4
`const unsigned char *`, 2 `const char *`, 1 `unsigned long *`, 3
`records::X *`) and the generated `GameData` declares each as a native
pointer (`using Snd_Device_type = void *;`, `symbols.gen.h:49463`), 8 bytes
on x86-64, which moves every later member. The round doc's "no pointer-typed
member" (section 0, the `GameData` row) counted `T *` spellings in the
`records*.h` structs, which hold none; the generated struct holds these.

Their use is wide: **8,690 uses in 483 live files** (section 14's script; a
count of the names outside `_addr` / `_type`, so an upper bound that includes
comments), `Sprite_Current` 5,103 of them, `Gfx_PacketNext` 1,350,
`Field_State` 656; **53 take the slot's address** (`&Snd_Device` into
`DirectSoundCreate`, `&Sprite_Current` into the harnesses, `&Gfx_PacketNext`).
The spelling that keeps every site: a generated **`bof3::Slot32<T>`** - a
4-byte struct holding `std::uint32_t v` with `operator T*()` (zero-extend),
`operator=(T*)` (through `Low32()`), `operator->` and `operator[]` for the
array-typed rows (`File_Slots[16]`, `Dat_FileNames[799]`, `Music_Events[2]`)
- emitted by `gen_symbols.py` for every pointer ctype on both widths. On the
32-bit build it is 4 bytes holding the pointer's bits, so the accessors'
machine code is unchanged (`codegen_same.py`'s gate; a trivial 4-byte
struct passed and returned by value has the same code as a pointer on i686,
the finding RB0 made for `Va`); on x86-64 it is the 32-bit slot the design
needs, and the `static_assert`s hold. The 53 address-of sites take a
`Slot32<T>*` where an out-parameter wants `T**`: the sound layer's `Create`
and `CreateSoundBuffer` take `Slot32<void>*` (6.3), the harness `&Sprite_Current`
sites are hosted. XA1's census counts these 111 as their own class.

## 10. The quicksave

- **`BuildId.dll_base`** (`snapshot.cpp:92`, `:130`, `:314`): the DLL at
  `0x10000000` is honoured (section 3), so the 32-bit word holds it; the
  word stays, the load's check (`:453-456`) refuses a relocated DLL as
  today. No format change.
- **The span** `Img(kDataVa)..Img(kOwnVa)` (`:201-203`) is the image's at
  `0x400000`: unchanged, 32-bit places.
- **The task arena** `[Task_StackTop - n, Task_StackTop)` (section 7.1): on
  the main thread's stack, 32-bit; `n` follows `kSlice64`, and the
  "at most `0x10000`" constant with it.
- **The keep list** (`kKept`, [`quicksave.md`](quicksave.md) 3): `Game_Hwnd`,
  `Game_HInstance`, the DirectInput objects, `D3d_Device` and the rest are
  32-bit slots holding our objects' or the process's low addresses (a 64-bit
  `HWND` is a handle below 4 GB by Windows' rule; `hinstance` is `0x400000`).
- **A 32-bit take on the 64-bit host** is refused by the DLL's sha256
  (`:453`) before any width question, as the round doc's 0.1 item 5 says.
  The `host` field (`kHostExe`) can stay: the sha separates the builds.
- **The SOUND participant** (`quicksave.md` 10): records `GetFrequency` and
  `GetCurrentPosition` answers and rebuilds buffers through `SndBuf_FromWave`;
  with the objects ours those answers are ours to make repeatable.

## 11. The crash reporter and the tools

| Site | Today | For the 64-bit host |
|---|---|---|
| `crash.cpp:52-54` `g_text_lo..g_image_hi` | `std::uint32_t` places | stay 32-bit (the image and the DLL are below 4 GB); `ImageRange` (`:269-273`) narrows through `Low32` |
| `crash.cpp:78` `Peek(std::uint32_t)`, `:85` `Where`, `:99` `InCode`, `:117` `AfterCall` | 32-bit addresses | `std::uintptr_t`: a fault in a system DLL (`0x7FFA...`) must be named, not truncated; `Where`'s module offset is `%lX` of a 64-bit difference |
| `crash.cpp:128` the faulting address, `:141`, `:148` the access address | narrowed from `ExceptionAddress` | native, printed `%016llX`, the VA beside it when in the image (`VaOf` on a place the image holds) |
| `crash.cpp:186-187` the stack scan | 4-byte words at 4-byte steps | 8-byte words at 8-byte steps on x86-64; `AfterCall`'s `E8` / `FF` test is the same bytes |
| `crash.cpp:233` `MiniDumpWriteDump` | the 32-bit dump | the 64-bit dump; `tools/crash_report.py:44-59` reads `Memory64ListStream` already, `:120-123` reads the PE's `ImageBase` as `<I` at `opt + 28`, which for a PE32+ is `<Q` at `opt + 24` (section 1.4's `pe_read.py` note; the tool reads `BOF3.exe` for bytes, a PE32, and the dump's module list for bases) |
| `tools/proc_image.py` | `EnumProcessModulesEx` for the main module's base, `move_of`, `place`, `va` | unchanged: `HMODULE` is pointer-sized in the tool's own Python, the base is `0x400000`, the linked span the same |
| `tools/task_stacks.py:116-128` `chain` | reads the slices as `<I` words at 4-byte steps, each a possible return into the image | on a 64-bit target the return addresses are 8-byte words into the DLL (`0x10000000..`), not the image: the reader takes the target's width from the exe's `magic` (`pe_read.py`) and steps 8; `after_call`'s byte test is the same |
| `tools/mem_dump.py`, `attract_watch.py`, `mem_watch.py` and the recipe `peek` / `poke` | VAs through `proc_image` | unchanged |

## 12. For the other groups

- **XA1:** count the 111 pointer-typed `[[data]]` entries and their uses
  (section 9) as a class of their own; the 53 address-of sites; the 14
  slots' readers; the `Method<>` sites by object (ours or Windows'); the two
  `fs:` reads of the harnesses beside `ImageBase()`'s.
- **XA3:** the x87 blocks are expected to assemble; the 38 control-word
  blocks hold no call (7.3). `mp3.cpp:178-188` is a stack switch for B1
  (7.6).
- **XA4:** `make_exe.py`'s PE32+ header and 64-bit entry stub, `pe_read.py`'s
  PE32+ fields and the four checks (1.4); the DLL's link options unchanged
  (`--disable-dynamicbase --image-base 0x10000000`, `.reloc` kept); the exe's
  `DllCharacteristics` without `DYNAMIC_BASE` and `HIGH_ENTROPY_VA`; SDL3
  built for x86-64 configures and links as a DLL from the fetched source
  with no change (`scratch/sdl3-x64`, 271 steps, `SDL3.dll`; the tree's
  static link is the same source).
- **B1:** the switch of 7.2 and the shape of 7.5; `mp3.cpp`'s block; the
  ten `_Bof3_ImageMove` operands become a RIP-relative load of a 64-bit base.
- **B2:** `Low32()` as written in 8.1; `Key()` and `Addr()` through it; `Img()`
  and `VaOf()` unchanged in width (8.1).
- **B3:** `Slot32<T>` for the 111 pointer ctypes (9), and the `unsigned long`
  rows as the round doc has them.
- **B4:** `ImageBase()` by width (8.1); `DetectHost` checks the PEB value
  against `GetModuleHandle` and `IMAGE_NT_HEADERS64`; `BindSlots` writes the
  thunks (4); `Crt_malloc` / `Crt_calloc` check their result (5); the bind
  table's reservation grows one page (4); `Task_SetStackBase` checks `rsp`
  (7.1); `crash.cpp` by 11.
- **B5:** `src/audio/sound_sdl.cpp` as 6.3, the slot binding, the WAV dump's
  tap; the state hash's expectations of 6.4; the listen is the owner's.
- **Stage C:** the stackwatch run that sets `kSlice64` (7.4); the five
  measurements of section 13.

## 13. Not verified, and the measurements missing

- That the game's own start-up keeps the bottom-up placement the probe saw:
  the launcher's child (the developer launcher reserves the linked span in
  the child, `launcher.cpp FenceLinkedSpan` - under option (a) the span is
  the executable's, so the reservation finds it taken and must skip it, as
  it does at `0x400000` today), the window, DirectInput's objects, the
  texture caches. `Low32` names the first miss.
- The real 64-bit frame depth on the routes: stage C's `BOF3X_STACKWATCH=1`
  run sets `kSlice64` (7.4); the sample's ratio is an estimate.
- Whether `Task_SetStackBase`'s `rsp` is below 4 GB in the game process as
  it is in the probe (the main stack's placement is the loader's; 7.1).
- The anonymous 16 MB section view at `0x1000000` in fixed-base processes:
  not identified (no file name); harmless to (a), named here so stage C is
  not surprised by it.
- SDL3 audio's latency and mixing against the DirectSound build: B5's WAV
  dump and the owner's listen.
- The pace cost of the x86-64 build's larger frames and the mixer: stage C's
  run 5.
- That `Slot32<T>` keeps `codegen_same.py` at `0 differ` on i686 (9): B3's
  first step.
- The counts of section 9 include names in comments (an upper bound); XA1's
  census is the authority.

## 14. Commands

```
# the probes (x86_64-w64-mingw32-clang++ on PATH), built and run in one directory
x86_64-w64-mingw32-clang++ -O1 -shared -o lowdll_probe.dll tools/probes/lowdll_probe.cpp -Wl,--image-base,0x10000000 -Wl,--disable-dynamicbase
x86_64-w64-mingw32-clang++ -O1 -o lowmem_noreloc.exe tools/probes/lowmem_probe.cpp -ld3d11 -lpsapi -Wl,--image-base,0x400000 -Wl,--disable-dynamicbase -Wl,--disable-reloc-section
x86_64-w64-mingw32-clang++ -O1 -o lowmem_fixed.exe   tools/probes/lowmem_probe.cpp -ld3d11 -lpsapi -Wl,--image-base,0x400000 -Wl,--disable-dynamicbase
x86_64-w64-mingw32-clang++ -O1 -o lowmem_aslr.exe    tools/probes/lowmem_probe.cpp -ld3d11 -lpsapi
./lowmem_noreloc.exe; ./lowmem_fixed.exe; ./lowmem_aslr.exe        # sections 1..4, 6.2 (SDL3.dll beside them for the SDL3 stage)
x86_64-w64-mingw32-clang++ -O2 -o taskstack_fixed.exe tools/probes/taskstack_probe.cpp -Wl,--image-base,0x400000 -Wl,--disable-dynamicbase -Wl,--disable-reloc-section
x86_64-w64-mingw32-clang++ -O2 -o taskstack_aslr.exe  tools/probes/taskstack_probe.cpp
./taskstack_fixed.exe; ./taskstack_aslr.exe                          # section 7

# SDL3 for x86-64 as a DLL, from the tree's fetched source (after cmake --preset i686)
cmake -S build/_deps/sdl3-src -B <dir> -G Ninja -DCMAKE_TOOLCHAIN_FILE=<an x86_64 twin of cmake/i686-llvm-mingw.cmake> \
      -DCMAKE_BUILD_TYPE=Release -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_INSTALL=OFF
cmake --build <dir>

# the frame growth (section 7.4): a 32-bit-slot copy of the generated header for the x86_64 compile (section 9)
cmake --build build --target bof3x_symbols bof3x_rebase
python tools/probes/frame_growth.py --all --gen64 <dir with bof3/symbols.gen.h, the 111 pointer slots as std::uint32_t> --out frames.tsv

# the allocation sites (section 5)
grep -rnE "(^|[^A-Za-z_])(malloc|Crt_malloc|VirtualAlloc|calloc|Crt_calloc)\(" src/game --include=*.cpp --include=*.h | grep -vi fuzz

# the control-word blocks with a call (section 7.3): a regex over every asm( / __asm__( block of the live files,
# a block counted when it names fldcw / fnstcw / ldmxcsr / stmxcsr, flagged when it also names `call`
# the pointer-typed [[data]] entries and their uses (section 9): tomllib over symbols.toml, a word-bounded
# grep of each name (not followed by _addr / _type) over the live files; the same with a leading `&`
```
