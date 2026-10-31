# The 64-bit build round: staging (PLAN phase 4, I44 "What follows")

**Status:** STAGED (2026-10-29; re-measured 2026-10-31 on `main` `43e98773`,
still not launched. The relocatable-image round it waited on is merged
(PR #168, with the state hash's pool fold as PR #170); section 0.1 lists what
that merge changed in this plan. The owner's calls in section 7 come first;
stage A needs none of them.)

The relocatable-image round ended with the source free of Capcom's
addresses: every data place is `Img(va)`, every code word is bound by name
through the bind table, the executable has no `/FIXED` base and no
entry-stub page. Its section 9 names what it leaves: "widening the records'
pointers: the 64-bit build (PLAN phase 4, I33's platform library)". This
round is that build: `bof3x.exe` as an x86-64 Windows executable, the same
source, the game's behaviour unchanged, proved by the state hash against the
32-bit references.

Section 1 argues that the "widening" I44 foresaw is not the cheap road and
need not be taken at all: the image's words can stay 32 bits wide inside a
64-bit process. That is the owner's call 1.

## 0. What pins the build to 32 bits today, measured 2026-10-31

On the checkout at `exe-gap/round` `d391bdee`, whose `src/`, `tools/`,
`CMakeLists.txt`, `cmake/`, `.github/` and `symbols.toml` are identical to
`main` `43e98773` (`git diff --stat 43e98773 HEAD -- src tools CMakeLists.txt
CMakePresets.json cmake .github symbols.toml` is empty). The 2026-10-29
figure is given where it moved, so the reader sees what the merge did. Fuzz
files (any file with `fuzz` in its name: 253 `.cpp` in `src/`,
`find src -iname "*fuzz*.cpp" | wc -l`) are counted apart: the shadow fuzz
runs in the hosted build only, which stays 32-bit for ever (section 2).

| What | Count, 2026-10-31 | Command or source |
|---|---|---|
| The toolchain's guard | `CMakeLists.txt:16` refuses any `CMAKE_SIZEOF_VOID_P` but 4; one preset, `i686` (`CMakePresets.json:5`); one toolchain file, `cmake/i686-llvm-mingw.cmake`; CI builds `i686` alone (`build.yml:33`) | the files |
| Toolchains on this machine | `x86_64-w64-mingw32-clang++.exe` and `aarch64-w64-mingw32-clang++.exe` beside the `i686` driver | `ls ~/.local/share/retcomm/toolchains/cmake-clang-v1/latest/bin` |
| Inline asm in live code (`src/`, fuzz files apart) | **174 blocks in 70 files** (the same total as 2026-10-29). By class: **104 x87 arithmetic** in 36 files (32 hold nothing else; `d3d_draw` 8, `d3d_rest` 8, `effect_1a` 7, `effect_3c` 7, `effect_3d` 7); **30 stack or control flow** in 15 files (`task_sched` 6, `sound_rest` 5, two each in `field_misc`, `mode_tasks`, `save_menu`, `scena_calls`, `shell`, `stackwatch`, one each in `mp3.cpp`, `area_slope`, `battle_misc`, `field_e2`, `field_modes`, `magic_s28`, `map_field_objects`); **23 control or status word only** in 11 files; **6 `fs:` reads** in 3 files (`boss_harness` 4, `scenario_harness` 1, `ImageBase()` 1); **11 other** in 10 files (7 `cltd; idivl`, 2 byte loads, `GlyphTrap`'s privileged `in` at `text_draw.cpp:44`, `gapwatch`'s load, `rest_1e.cpp:180`'s empty register barrier). 6 asm-label aliases (`__asm__("_Music_Play")` and five more, `sound.cpp` 5, `sound_rest.cpp` 1) | a classifier over every `asm(` / `__asm__(` block after stripping comments. A block is **stack** if its text has `push`, `pop`, `call`, `jmp`, `ret`, a `j` branch, `esp` or `ebp`; else **fs** if it reads `fs:`; else **x87** if any mnemonic is an x87 operation other than `fnstcw` / `fldcw` / `fnstsw`; else **control word** if every mnemonic is `fnstcw`, `fldcw`, `stmxcsr` or `ldmxcsr`; else **other**. The 2026-10-29 split (125 / 29 / 20) used a classifier that was not kept, so its classes differ from these; XA1 makes this one a tool |
| Asm that names a C symbol by its x86 spelling | 39 operands in 16 live files name a symbol with the leading underscore of the i686 ABI (`call _savemenu_musicplayingbody`, `jmp *(_soundrest_g+8)`), which x86-64 does not prefix. **10 of them, new since 2026-10-29**, load `_Bof3_ImageMove` by absolute address (`task_sched.cpp` 8, `field_e2.cpp:126`, `field_modes.cpp:140`; the variable at `host.cpp:25`), which x86-64 cannot encode outside the low 2 GB without a RIP-relative form | `grep -rhoE "(call\|jmp\|movl\|subl\|addl\|cmpb\|jne\|je\|testb)l? +\*?\(?_[A-Za-z][A-Za-z0-9_]*"` over the live files that hold asm; `grep -rn "_Bof3_ImageMove" src` |
| Naked functions | **69 in 31 files**: 42 in 18 fuzz files, **27 in 13 live files** (`task_sched` 6, `sound_rest` 5, two each in `shell`, `scena_calls`, `save_menu`, `mode_tasks`, `field_misc`, one each in `map_field_objects`, `magic_s28`, `field_modes`, `field_e2`, `battle_misc`, `area_slope`). The 2026-10-29 "40 in 16 live files" does not reproduce on its own tree with this command (27 in 13 on `628aa521`); the census replaces it | `grep -rlE "__attribute__\(\(([a-z_, ]*)naked" src`, counted per file |
| Calling-convention spellings | `__cdecl` 16,160, `__stdcall` 305, `WINAPI` 24, `CALLBACK` 19; no `__fastcall`, `__thiscall` or `regparm`. All are ignored on x86-64; what depends on them is the asm above | `grep -rhoE "__stdcall\|__cdecl\|__fastcall\|__thiscall\|WINAPI\|STDMETHODCALLTYPE\|CALLBACK" src \| sort \| uniq -c` |
| A native address stored into a 32-bit slot | **384 sites** (382 on 2026-10-29, **+2**): 230 in 170 live files, 154 in 114 fuzz files. By argument: 196 `p`, 23 `g_prims`, 8 `f` (11 before), 5 `callee`, 3 `entry`, 3 `code`, 3 `m`. `bind.cpp:22`'s `Key()` narrows every function of ours that the bind table jumps to. The earlier "19 take a function" was a hand read; XA1 classifies again | `grep -rhoE "static_cast<std::uint32_t>\(reinterpret_cast<std::uintptr_t>\(" src \| wc -l`; the same on `41372e3c` gives 382 |
| A 32-bit value used as a native address | **950 sites** (933 was `main` `628aa521`, before the round; 948 on the round's tip; **+17** over the old figure): 639 in 285 live files. 1,807 spellings of `std::uintptr_t` (1,785, **+22**), 1,141 of them live; `sizeof(void *)` twice (unchanged) | `grep -rhoE "reinterpret_cast<[^>]+>\(static_cast<std::uintptr_t>\(" src \| wc -l`; `git grep` of the same on `628aa521` |
| `Img()` places | **14,977** (14,976, **+1**), 1,070 of them in fuzz files; `VaOf()` 50 (unchanged); `Code(` 617 spellings. `Img`, `VaOf` and `ImageBase` take and return `std::uint32_t` (`src/common/image_base.h:60`, `:69`, `:73`); `ImageBase()` reads the PEB's `ImageBaseAddress` through `fs:0x30` + 8 (`image_base.h:60-66`), an x86 layout (x86-64: `gs:0x60` + 0x10) | `grep -rhoE "\bImg\(" src \| wc -l`, the same for `VaOf(` and `Code(` |
| Code words and calls through an address | 8,784 `.text` words, bound by name since stage C2; the bind table has 10,088 rows; `gate_abs_call` 1,340 run-time calls through an address, `call_through_data_word` 745 | `python tools/reloc_census.py --counts`; `symbols.toml` (10,088 `impl` of 10,100 `[[func]]`) |
| The bind table's rows | each row is a 5-byte `jmp rel32` at `(top << 24) \| VA` in a table the process reserves under a measured top byte 0x7C, 0x70, 0x7E, 0x7B or 0x7D, then any 0x20..0x7F (`bind.cpp:72`, `BuildTable` at `:98`); rows are 11 bytes apart at the nearest. A 64-bit absolute jump needs 12 bytes or more, so the 5-byte row survives only if our code lies within 2 GB of the table | `src/hook/bind.cpp` |
| Where our code lies | `bof3x.dll` linked at `0x10000000` with `--disable-dynamicbase` (`CMakeLists.txt:768`); `bof3x.exe` is not linked but written by `tools/make_exe.py` as a PE32 at `0x400000` (magic `0x10B`, machine `0x14C`, `make_exe.py:389`, `:400`) whose sections are the image, importing `bof3x.dll!Engine_Main`. One `bof3x.dll` serves both hosts (`image_base.h` header comment) | the files |
| The import slots | **14 four-byte slots** in the image that the exe host fills with a system DLL's function (`exe_host.cpp:407` `kSlots`; `BindSlots` at `:435` copies 4 bytes, `:438`): DSOUND ordinal 1, three GDI32, six KERNEL32, four USER32. In a 64-bit process those functions lie above 4 GB | the file |
| The unsigned longs | 1,333 spellings of `unsigned long` in `src/game` (1,331, **+2**), 1,528 in `src/`. The generated ctypes: 1,745 `unsigned long`, 42 `long`, 1 `unsigned long *` of 3,165 `[[data]]` ctypes (3,249 rows) | `grep -rhoE "\bunsigned long\b" src/game \| wc -l`; `tomllib` over `symbols.toml`'s `[[data]]` |
| The records | 43 structs in `src/game/records*.h` (23 + 8 + 12), each sized by a `static_assert` (43); **no pointer member**: "Address fields are std::uint32_t: the image is 32-bit whatever the host" (`records.h:4`); at least 9 `std::uint32_t` members are documented as an address or a pointer by their comment. The 2026-10-29 count of 39 such fields came from a wider read, not re-measured | `grep -hcE "^\s*struct " src/game/records*.h`; `grep` for a `T *` member (none) |
| The data image as a struct | `GameData` is `0x37B000` bytes (`static_assert` in the generated header), **2,968 named members** (2,965 before) and 871 pads, **no pointer-typed member** | a brace-depth parse of `build/gen/bof3/symbols.gen.h` (the build of 2026-10-30, `symbols.toml` last changed the same day) |
| The typed-data census | 27,427 raw data sites, 9,059 record sites, 17,239 global sites, 8,916 in fuzz files | `python tools/data_census.py --counts` |
| Allocation in `src/game` | **47 sites, unchanged**: 20 `malloc`, 8 `Crt_malloc`, 14 `VirtualAlloc`, 3 `Crt_calloc`, 2 `calloc`; **29 of them in live files** (20, 6, 1, 1, 1); 8 more in `src/hook`, `src/render`, `src/audio`, `src/common` (1 `malloc`, 7 `VirtualAlloc`) | `grep -rhoE "(^\|[^A-Za-z_])malloc\("` and the same per name, live and all |
| The task stacks | four slices of `0x4000` on the process's main thread stack; `Task_StackTop` a 32-bit slot (`symbols.toml:803`) and each record's saved `esp` a 32-bit word; the switch is naked asm (`task_sched.cpp` 1..26); four slices need `0x14294` bytes below base on the 32-bit build | `symbols.toml:803`, [`quicksave.md`](quicksave.md) 1, [`state3-design.md`](state3-design.md) 5 |
| Other stacks | the present runs on a fiber with a 1 MB stack (`render_shim.h` header comment; `ConvertThreadToFiber` 3, `CreateFiber` 4); `mp3.cpp`'s decoder switches `esp` in asm | `grep -rhoE "ConvertThreadToFiber\|CreateFiber" src` |
| Where the packets may lie | below 16 MB: the ordering table's links are 24-bit (`d3d_list.cpp:209-210`), DIV-0062's pool asserts it, and `host.cpp:54-58` refuses an image that ends past `0x1000000`. The legal moved bases are `0x990000..0xA70000` | [`relocatable-image-design.md`](relocatable-image-design.md) 1.2, [`relocatable-image-round.md`](relocatable-image-round.md) 4 |
| The image span | `.data` at `0x5C4000`, `0x37B000` bytes; `.bss` zeroed to `0x93D6EC` | [`relocatable-image-round.md`](relocatable-image-round.md) 0 |
| The Direct3D and DirectDraw objects | **ours**: `render_shim.cpp` builds DirectX 6-shaped objects with x86 layouts (`DDSURFACEDESC2` as `0x7C` bytes, every method `__stdcall` "as COM is on x86", `render_shim.cpp:1-4`) and the game's draw code calls them through their vtables; `IDirectDraw*` named 24 times in 10 live files, `IDirect3D*` 36 times in 16 | `grep -rhoE "IDirectDraw[A-Za-z0-9]*\|DirectDrawCreate[A-Za-z]*"` (and `IDirect3D`) over the live files |
| The DirectSound objects | **Windows' own**: `DirectSoundCreate` through the import slot `0x5C4014` (`exe_host.cpp:408`); the device, primary, music stream, notify and SND stream buffers are kept in 32-bit slots of the image (`Snd_Device`, `Music_Buffer` and the rest, `symbols.toml:8889-8930`); 54 vtable calls through `Method<>()` in 5 sound files (`sound.cpp` 32, `sound_rest.cpp` 9, `battle_items.cpp` 7, `sound_sp.cpp` 5, `sound_callees.h` 1); `Snd_BufferDesc` is the 32-bit `DSBUFFERDESC` (nine words, `dwSize` `0x24`, `symbols.toml` `Snd_Init`'s evidence), which on x86-64 holds a pointer and is larger | `grep -rhoE "\bMethod<?[^(]*\(" src/game`, counted per file |
| The rest of the Windows surface | `d3d11` included in 2 files (`src/render`), `dinput` in 4 (3 `src/game`, 1 `src/input`; `pad_read.cpp:241` loads `dinput.dll`), `mmsystem` in 2 (`src/hook` for the FMV's MCI, DIV-0035; the launcher); the DLL links `d3d11 dxgi d3dcompiler_47 uuid winmm bcrypt ole32 windowscodecs dinput dxguid SDL3::SDL3-static` (`CMakeLists.txt:758`); **SDL3 3.4.16 already fetched** for the pad (DIV-0050, `CMakeLists.txt:93-112`, [`THIRD_PARTY.md`](THIRD_PARTY.md) 2) | `grep -rlE "#include\s*[<\"]<name>(\.h)?[>\"]" src` per header |
| C++ floating point | already SSE in the `i686` build: the DLL's code has 9,551 `movss`, 1,129 `mulss`, 893 `addss` against 557 `flds` and 994 `fstps` (the asm blocks, and the i686 ABI's float returns in `st(0)`). So the x86-64 build's SSE arithmetic is the 32-bit build's, outside the asm | `llvm-objdump -d --no-show-raw-insn build/bof3x.dll`, mnemonics counted (the build of 2026-10-30) |
| The log formats that print a place | `%08X` 366, `%p` 67 | `grep -rhoE "%08[Xx]\|%p" src \| sort \| uniq -c` |
| The FMV codec | `capcom.avi` shipped as Indeo 5, which no 64-bit Windows decodes; already re-encoded to Cinepak by the ledger's entry "Re-encode `capcom.avi` from Indeo 5 to Cinepak" ([`DIVERGENCE.md`](DIVERGENCE.md) line 279) | the ledger |
| The quicksave's build check | the quicksave's `BuildId` holds the host, the version tag, the **sha256 of `bof3x.dll`** and the DLL's base as a 32-bit word (`snapshot.cpp:126-131`, filled at `:300-319`). A quicksave from another DLL is refused already, so a 32-bit take is refused by a 64-bit build without new code; `dll_base` is the one field that must widen or stay low | the file |
| The state hash | the tool reads `BOF3SH1`..`BOF3SH5` (`tools/statehash.py:34-75`); since PR #170 a folded `BOF3SH5` run refuses a `BOF3SH4` folded file by design; the hash maps a code word back to its VA only inside the bind table (`bind.cpp` header comment) | [`state-hash.md`](state-hash.md) 8, [`statehash-pool-fold.md`](statehash-pool-fold.md) |
| **The low-memory probe** (a 64-bit program built with the `x86_64` driver; **not re-run** on 2026-10-31) | 2026-10-29: `VirtualAlloc(0x400000, 0x600000)` succeeds; the window `0x10000..0x1000000` is entirely free before our block; 64 MB at `0x1000000` succeeds. The same program linked by lld with `--image-base 0x400000` was moved by ASLR to `0x4E0000` and the block then failed (error 487). Not measured: a PE32+ that `make_exe.py` writes with no relocations and no `DYNAMIC_BASE`, as `bof3x.exe` is today | `tools/probes/lowmem_probe.cpp` |

Three consequences:

- **The game's words never need to widen.** No record and no `GameData`
  member holds a pointer; a pointer field is a 32-bit VA slot already. What
  the 64-bit build must do is keep every address the game can see
  representable in 32 bits: the image, the packets, the heap, the task
  stacks, our functions (the bind table's `Key()`), the objects the game
  keeps a pointer to (the DirectSound buffers, the render shim's objects),
  and the functions behind the 14 import slots.
- **Most of the asm survives x86-64 as written.** x87 exists on x86-64 and
  the 104 arithmetic blocks and 23 control-word blocks use memory operands
  and `st`; they should assemble under the `x86_64` driver unchanged, which
  XA3 confirms by building, not by assertion. The 30 stack and control
  blocks, the 27 live naked functions, the 6 `fs:` reads, the 39 underscored
  symbol operands and the 6 asm-label aliases do not: `esp`, `pushl`, the
  cdecl stack, `fs:` and the leading underscore are 32-bit by nature.
- **The proof already exists.** With the image's words 32 bits wide, a page
  of the 64-bit build's image is byte for byte what the 32-bit build's is,
  up to the pointer normalisation RA2 built ([`state-hash.md`](state-hash.md)
  8), provided the bind table keeps the same `(top << 24) | VA` shape. The
  `cn_<route>_orig4` pairs and their `_orig4r` re-hashes compare directly.

### 0.1 What the merged relocatable-image round changed in this plan

The 2026-10-29 draft was written against the round's branch. Since the
merge:

1. **Code tokens are settled.** XA2 was to choose between a low thunk table
   and keeping the VA as the token. Stage C2 built the first: the bind
   table (`bind.cpp`), one `jmp` per bound VA under a top byte, so a stored
   code word is a real address whose low three bytes are Capcom's. What is
   left for this round is the **reach**: a 5-byte `jmp rel32` row reaches
   our code only if the 64-bit DLL lies within 2 GB of the table, and
   `Key()` holds our function's address in 32 bits only if the DLL lies
   below 4 GB. The 32-bit DLL's fixed base `0x10000000` meets both. Whether
   a 64-bit DLL at a low fixed base loads there is a probe for XA2.
2. **The image is a module, not a block.** `bof3x.exe` is a PE32 written
   by `make_exe.py`, with the image as its own sections, and `ImageBase()`
   reads the PEB. The draft's "VirtualAlloc the image at `0x400000`" is now
   one of two options: (a) `make_exe.py` writes a PE32+ at `0x400000` as
   today, `ImageBase()` reads `gs:0x60` + 0x10; (b) a high 64-bit
   executable maps the image as a block, and `ImageBase()` becomes the
   block's address, not the PEB's. XA2 measures (a) first, because it
   changes the least.
3. **The asm names the image's move.** Ten asm operands load
   `_Bof3_ImageMove` by absolute address (stage C1's way of keeping the
   original's register contract at a moved base). B1 owns them.
4. **The import slots and the DirectSound objects** were not in the draft.
   Both are native addresses of Windows' code and objects kept in 32-bit
   words of the image (section 0). The slots want a low thunk each, which
   is the bind table's shape. The sound objects want a design: section 3's
   XA2 and the owner's call 4.
5. **The quicksave's cross-width question is answered.** The `BuildId`'s
   DLL hash refuses a quicksave taken by another build, so a 32-bit take is
   refused on the 64-bit host already.
6. **`BOF3X_ORIGINAL` is refused already.** The exe host ends the process
   with a message when `BOF3X_ORIGINAL` or `BOF3X_SHADOW` is set
   (`exe_host.cpp:82-86`). The 64-bit host inherits this; the draft's "logs
   once and runs ours" is withdrawn (section 9).
7. **The state hash's formats moved.** `BOF3SH5` and the pool fold (PR
   #170); the proof references are named by section 5 from
   [`state-hash.md`](state-hash.md) 8.

## 1. The two roads, and the one to take (the owner's call 1)

| | **(N) The narrow image** (recommended) | (W) The wide image |
|---|---|---|
| The image's words | 32-bit, the layout BOF3.exe's, loaded as today | every pointer slot widened to 64 bits at load; `GameData` regenerated with new offsets |
| Where the image lies | at `0x400000` below 16 MB: a PE32+ module or a block (section 0.1 item 2; the probe) | anywhere |
| The game's run-time pointers | must lie below 4 GB (packets below 16 MB): a low heap, low task stacks, our DLL low, low thunks for the import slots, low or shimmed DirectSound objects | native, anywhere |
| `Img(va)` | returns a native pointer; `VaOf()` narrows with a check | the table maps VA to a widened place; every record-offset site (9,059 `sites_record`, 27,427 raw sites by `data_census.py`) re-derived |
| The saves, the quicksave, the cache, the PSX-layout data files | unchanged: they hold the 32-bit layout, which is still the layout | every format translated; the quicksave's native span (DIV-0145..0147) redesigned |
| The state-hash references | compare directly, pointer slots as offsets | a new normalisation and new references; no 32-bit against 64-bit comparison |
| The hosted fuzz against Capcom's code | unchanged, in the 32-bit hosted build | unchanged |
| Lines changed | the loader, `image_base.h`, the 384 narrowing sites, the asm of section 0, the sound layer's object pointers | those, and every typed accessor's offset, every record struct, every tool that reads a save or a dump |
| What it does not give | records with native pointers in "reconstructed source" | - |

**Recommended: (N).** It is the build the proof can see. (W) is I44's
"same table applied once more" and it is not: the table rebases words, it
does not move every field behind them. Nothing the project wants from 64
bits (a modern toolchain's default target, more address space for our own
caches, the step toward the platform library) needs the game's own words
wider. If a later round wants native pointers in the records for
readability, it starts from (N) with the proof in hand.

(N) has one dependency on the operating system: a program may map memory
below 16 MB and below 4 GB. Measured true on Windows 11 here (section 0's
probe, 2026-10-29). On Linux `mmap_min_addr` is 64 KB by default; on macOS
the default `__PAGEZERO` covers the low 4 GB and the linker can shrink it.
Neither is this round's target (section 9); both are a one-line probe when
they are.

## 2. The method: one source, two builds, the 32-bit build the oracle

The typed-data and relocatable-image rounds proved their spellings with
`tools/codegen_same.py` (machine code identical) and their moves with the
state hash. Both gates stay, and a third joins:

1. **Every source change lands in the `i686` build first** and is judged
   there. A pure spelling (a narrowing helper in place of a cast, a
   `std::uint32_t` in place of an `unsigned long`) keeps `codegen_same.py` at
   `0 differ`. An asm block rewritten as C++ changes the machine code, so it
   is proved the way every takeover was: the **hosted shadow fuzz against
   Capcom's function** (the 253 fuzz files, `BOF3X_SHADOW='*'`, narrow and
   wide), which exists only in the 32-bit hosted build - the reason the
   hosted build keeps its base and width for ever. The exe host refuses
   `BOF3X_SHADOW` (`exe_host.cpp:85-86`), so the fuzz never runs on the
   64-bit build.
2. **The `x86_64` build compiles the same source.** What differs by width
   is behind `sizeof(void *)` or the preprocessor in a handful of headers
   (`image_base.h`, `task_sched`'s switch, the stackwatch, the bind table's
   row), never in a game file. A game file that needs `#if` by width is a
   defect of the design, listed in the stage D audit.
3. **The 64-bit executable is proved by the state hash**: the attract
   sequence and the ten routes, the `x86_64` exe host against the
   references of section 5, identical pair for pair; the exe-host
   self-tests (`en-US`) headless on it; and the Rand counts the references'.

The order follows from this: stage B's rewrites are 32-bit work with the
old gates, and only stage C builds and runs the 64-bit executable. Stage B
is therefore safe to merge to `main` before any 64-bit binary exists.

## 3. Stage A: census, design, the build (four groups, in parallel, no `src/game` change)

### XA1: the census of what assumes a width
`tools/width_census.py`, written to run in CI like `reloc_census.py`: every
site in `src/` that assumes a pointer is 32 bits, by class -

- a native address narrowed into a 32-bit slot (the 384 of section 0, each
  read: is the address one the game stores, or ours alone?);
- a 32-bit word used as a native address (the 950);
- `long` and `unsigned long` where the width matters (Win64 is LLP64, so
  these are 32 bits there; the census marks them for the LP64 targets of
  section 9 and for the generated ctypes);
- inline asm, by the five classes of section 0 (the classifier's rules
  written down there), with the file's fuzz or live standing;
- naked functions, asm-label aliases, symbol operands with the i686
  underscore, `_Bof3_ImageMove` operands, `__cdecl` / `__stdcall` spellings
  (ignored on x86-64, listed to be sure nothing depends on them);
- `fs:` reads, `VirtualAlloc` with an address, `sizeof(void *)`,
  `std::uintptr_t`, the `%p` and `%08X` log formats that print a place;
- the slots of the image that hold an address of Windows' (the 14 import
  slots, the DirectSound object slots) and of ours (the render shim's
  objects).

Output a TSV under `analysis/` and the counts in this doc. It sizes stage
B's groups and stage D's audit.

### XA2: the low-memory design
A design doc in the shape of [`relocatable-image-design.md`](relocatable-image-design.md),
answering with measurements:

- **the image**: option (a), `make_exe.py` writing a PE32+ (magic `0x20B`,
  machine `0x8664`) at `0x400000` with no relocations, as today's PE32 is -
  does Windows load it there, and what `pe_read.py` checks; or option (b),
  a high executable and the image as a `VirtualAlloc` block before any heap
  or thread of ours exists (the probe says the window is free at `main`;
  the design says at what point of start-up it is still free and what
  `Fatal` says when it is not). `ImageBase()` by width either way;
- **our code**: the 64-bit `bof3x.dll` at a fixed low base (the 32-bit
  one's `0x10000000`, `CMakeLists.txt:768`) so that `Key()` fits 32 bits and
  the bind table's 5-byte rows reach it. A probe: does the loader honour a
  64-bit DLL's low preferred base with `DYNAMIC_BASE` off, and is it then
  within 2 GB of every top byte the table may take? If not, a row becomes a
  `jmp rel32` to a nearby 14-byte absolute jump, and the measured 11-byte
  gap between rows is checked against that;
- **the import slots**: a low thunk per slot, in the bind table's reserved
  range or beside it, so the 14 words stay 32 bits;
- **the heap the game sees**: which of the 29 live allocation sites hand an
  address to the game (into a record, a packet, a `GameData` slot) and which
  are ours alone; a low arena for the first class (`VirtualAlloc` at
  `0x1000000`, measured; its size from the game's peak, `BOF3X_DATLOG` or a
  counting allocator over the routes), the toolchain's heap for the second;
- **the objects the game keeps a pointer to**: the render shim's objects
  are ours, so they move into the low arena; the DirectSound objects are
  Windows', so either a DirectSound-shaped shim of ours in front of the
  64-bit `dsound` (as `render_shim` is in front of Direct3D 11), translating
  the 32-bit `DSBUFFERDESC` and the other descriptors the game builds, or the
  sound layer's output moved to the platform library (call 4). The design
  measures which is smaller;
- **the task stacks**: four `0x4000` slices today on the main thread; in
  the 64-bit build a low arena of our own, slice size from a stackwatch
  measurement of the 64-bit frames (the 32-bit need is `0x14294` for four,
  [`state3-design.md`](state3-design.md) 5; 64-bit frames are larger), and
  the switch written for `rsp` with the x86-64 callee-saved set (`rbx`,
  `rbp`, `rsi`, `rdi`, `r12`..`r15`, `xmm6`..`xmm15`, and the x87 state the
  switch never saved because the original never needed it - measured, not
  assumed). The present's fiber and `mp3.cpp`'s decoder stack are measured
  the same way;
- `Img()` as a native pointer, `VaOf()` as a checked narrowing;
- the quicksave: `BuildId.dll_base` by width, or the DLL fixed low so the
  32-bit word holds it; the span at a fixed base in both widths
  ([`quicksave.md`](quicksave.md) 1 and 3);
- the crash reporter's addresses (the VA beside a faulting address in the
  image, as stage C1 made it) and the `tools/` that read a dump
  (`tools/proc_image.py`).

### XA3: the asm policy, settled by a build
Compile every live file that holds asm with the `x86_64` driver, in
isolation, with a stub header that defines what the file includes. Report:
which of the 104 x87 and 23 control-word blocks assemble unchanged
(expected all: memory operands, `st`, no general register of a 32-bit
name), which of the 30 stack and control blocks, 6 `fs:` blocks and 11
others fail and what each does in words, which of the 27 live naked
functions are reachable in the exe host at all (several exist only for a
Capcom caller's register contract, and in the exe host every caller is
ours). The report is stage B1's list. **An x87 block that survives stays
asm** in this round: the x87 semantics (the control word, 80-bit
intermediates, `fistp` under chop) are what the original computes, and a
C++ rewrite is a divergence until fuzzed; the round that leaves x86
altogether (section 9) owns that question. The C++ outside the asm is SSE
in both builds already (section 0), so it needs no such care.

### XA4: the build
A second preset `x86_64` and `cmake/x86_64-llvm-mingw.cmake`; the guard at
`CMakeLists.txt:16` becomes a choice of the two; the hosted targets (the
injected DLL's hosted mode, the fuzz) built for `i686` alone, the exe host
and the launcher parts for both; `make_exe.py` and `pe_read.py` taught the
PE32+ header (or the high executable, by XA2's answer); SDL3 and minimp3
built for the width; a CI job `x86_64` that configures and compiles
(nothing runs in CI, rule 1) and uploads `bof3x-x86_64-<sha>`. Until stage
C the `x86_64` job is allowed to fail at link; its compile log is XA3's
second opinion.

## 4. Stage B: the rewrites, in the 32-bit build, gated (file groups, after XA1 and XA3 merge)

Each group owns its files, resets onto the round branch's tip, runs the
gate that applies, and reports. Merge one at a time.

- **B1, the stack and control asm** (from XA3's list; `task_sched`,
  `sound_rest`, `shell`, `scena_calls`, `save_menu`, `mode_tasks`,
  `field_e2`, `field_misc`, `field_modes`, `area_slope`, `battle_misc`,
  `magic_s28`, `map_field_objects`, `mp3.cpp`, `stackwatch`, the `fs:` reads
  in `boss_harness` and `scenario_harness`, the ten `_Bof3_ImageMove`
  operands, the 39 underscored operands and the 6 aliases). Each block
  becomes one of: C++ that the hosted fuzz proves against Capcom's
  function; a width-conditional pair (`i686` asm kept byte for byte,
  `x86_64` asm beside it) where the block is a stack switch no C++
  expresses; or a thunk without `naked` where the function only re-arranges
  arguments for a Capcom convention (the eax-result aliases).
  `codegen_same.py` for the files whose 32-bit code is meant to be
  unchanged, the fuzz for the rest.
- **B2, the narrowings**: the 384 sites through one helper, `Low32(p)`,
  which on the 32-bit build is the cast (`0 differ`) and on the 64-bit
  build checks the address lies below 4 GB and is `Fatal` otherwise - the
  loud trap for an address the design of XA2 missed; `bind.cpp`'s `Key()`
  the same; the 950 widenings through `Img()` or the helper's inverse; `%p`
  prints of places as VAs.
- **B3, the types**: the generated ctypes' `unsigned long` and `long` as
  `std::uint32_t` / `std::int32_t` in `tools/gen_symbols.py`'s output, and
  the 1,333 spellings in `src/game` where the census says the width is the
  image's (`0 differ` on Win32, where `long` is 32 bits; this group is for
  the LP64 targets and for the reader).
- **B4, the loader and the helpers**: `image_base.h` by width,
  `exe_host.cpp`'s start-up (the image by XA2's option, the low arena, the
  task-stack arena, the import slots' thunks), `bind.cpp`'s row by XA2's
  reach answer, `host.cpp`'s identity for a 64-bit exe host,
  `snapshot.cpp`'s build id. Compiled for both widths; the 32-bit build's
  code identical (`0 differ`) or fuzzed, since the 32-bit exe host must not
  change behaviour.
- **B5, the sound objects**: XA2's answer for the DirectSound pointers
  (the shim, or the owner's call 4's library), in the 32-bit build first,
  where the shim in front of the 32-bit `dsound` must give the same sound
  (the state hash's sound windows and a listen by the owner).

Stage B merges to `main` as a PR of its own when every group is in: the
32-bit build is proved by its own gates, and the 64-bit build is not yet
claimed.

## 5. Stage C: the first 64-bit run (one agent, then the coordinator's runs)

The agent builds the `x86_64` preset to a linked `bof3x.dll` and a written
`bof3x.exe`, fixes what the link finds (an import, a symbol the asm-label
aliases named with a leading underscore that x86-64 does not use), and runs
the exe-host self-tests headless under the round's harness root. The
coordinator then runs:

1. the attract sequence and the ten routes on the 64-bit exe host at
   `0x400000`, the state hash against each route's `cn_<route>_orig4` pair
   and its `BOF3SH4` re-hash `cn_<route>_orig4r`
   ([`state-hash.md`](state-hash.md) 8; the save-list pages are the owner's,
   as stage C of the relocatable-image round read them), and against a
   32-bit exe-host run of the same commit the same day by `statehash.py
   diff`: identical but the noise class, the Rand counts the references';
2. the quicksave: a take and a load on the 64-bit host (works); a 32-bit
   take loaded on the 64-bit host (refused by the build id, section 0);
3. the FMV: `capcom.avi` plays through MCI on the 64-bit host (the Cinepak
   re-encode is the ledger's; a 64-bit `iccvid` is Windows' own, to be seen
   not assumed), and the two other AVIs;
4. the launcher: launches the 64-bit executable from the Sources page as it
   does the 32-bit one, the `bof3x.exe` path a setting; the 32-bit launcher
   may launch either (the developer launcher's span fence reserves memory in
   the child, which from a 32-bit launcher into a 64-bit child is to be
   seen);
5. the pace: the attract's logic frames in 360 s on the 64-bit host against
   the 32-bit host the same day ([`state-hash.md`](state-hash.md) 1; the
   relocatable-image round saw 9,106 to 10,789 across runs with the
   machine's load, so the comparison is same-day, back to back).

A difference in 1 is a defect of stage B or of XA2's design and is fixed on
the round branch before anything else merges.

## 6. Stage D: audit and docs

A read-only audit of every width-conditional line in `src/` (the list is
XA1's census re-run on the round's tip: the count of `#if` by width in a
game file should be zero), of every `Low32()` that fires in no run but
could, and of the ledger: a 64-bit build that behaves as the 32-bit one
needs no DIV entry; anything stage C changed for the player (a refused
quicksave, a `Fatal` on low memory, a sound the shim changed) gets one.
`STATUS.md`, `HANDOFF.md`, [`SCAFFOLDING.md`](SCAFFOLDING.md) 4 (the
presets), `release.yml` (the second artefact, signed as the first), this
doc's "as run" sections.

## 7. For the owner

1. **The road: (N) the narrow image, or (W) the wide one** (section 1). The
   proposal is (N). Stage A is useful to both; (W) adds a stage B of the
   typed-data round's size and a new proof.
2. **The target set for this round.** Proposed: x86-64 Windows alone. The
   x87 blocks stay asm because x86-64 has an x87; the first non-x86 target
   (an ARM64 Windows handheld, Apple silicon) needs an x87 model in software
   or a fuzzed C++ for each block, and the Linux and macOS targets need the
   platform library (I33's decision 3). That is a round of its own; naming
   it now only decides whether B1 writes its stack switch with a third
   variant in mind.
3. **What the 64-bit executable is for, at first.** A developer's build and
   a second release artefact beside the 32-bit one (proposed), or the
   player's default. The second needs a release of play on it first; the
   launcher can offer both from one Sources page.
4. **The platform library, early decision 3 of I33.** SDL3 is already in
   the tree for the pad (DIV-0050, zlib). Confirming it as the library for
   window, audio output and timing later is not this round's work, but B4's
   loader and B5's sound objects are written once and should not be written
   against an API the next round removes. The re-measure adds weight here:
   the DirectSound objects are the one Windows API whose pointers the game
   keeps, so a DirectSound shim (B5) is work that SDL3 audio would replace.
   A one-word answer is enough.

## 8. Ownership, merge order, numbering

| File or area | Owner |
|---|---|
| `tools/width_census.py`, `analysis/width_census.tsv` | XA1 |
| `docs/x64-build-design.md` | XA2 |
| XA3's report (no source) | XA3 |
| `CMakePresets.json`, `cmake/x86_64-llvm-mingw.cmake`, `CMakeLists.txt`, `build.yml`, `tools/make_exe.py`, `tools/pe_read.py` | XA4 |
| each source file | its stage B group, one owner |
| `src/common/image_base.h`, `src/hook/exe_host.cpp`, `bind.cpp`, `host.cpp`, `snapshot.cpp` | B4 |
| the sound layer's object slots and any DirectSound shim | B5 |
| the 64-bit link and the first runs | stage C |
| `STATUS.md`, `HANDOFF.md`, reference runs | the coordinator |

Merge order: XA1, XA2, XA3, XA4 (any order, no shared files); B1..B3 one at
a time with the gate after each; B4; B5; stage B's PR; stage C; stage D.
Every commit `git commit -s`. Agents run no game: the coordinator runs the
self-tests (hosted `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*'` beside the
exe-host and moved runs, HANDOFF's "How to run things") and the proof, one
at a time, in this round's own harness root under
`C:\Users\kerok\bof3x-runs\x64<mmdd>` (Defender-excluded, one root per
session). Worktrees reset onto the round branch's tip, not `main`.

Numbering: ledger entries continue from the first free number after
DIV-0191, and defects from the first free number after D398, both checked
against `main` at launch. A 64-bit build that changes no player-visible
behaviour needs no ledger entry; anything it does change does.

## 9. Not in this round

- Widening the image's words (road W), unless call 1 picks it.
- Any target but x86-64 Windows: ARM64, Linux, macOS, and with them the x87
  model and the platform library's window, audio and timing (I33).
- `BOF3X_ORIGINAL` and every switch of Capcom's code on the 64-bit host:
  Capcom's code is 32-bit and cannot run in the process. The exe host
  already refuses `BOF3X_ORIGINAL` and `BOF3X_SHADOW` with a message
  (`exe_host.cpp:82-86`), and the 64-bit host keeps that refusal; the
  ledger's `Switch:` fields that name it hold for the hosted build. This
  round adds no player-facing text.
- The hosted build and the fuzz: 32-bit at `0x400000` for ever.
- I5's translation-unit layout (after this, per the owner's 2026-10-21
  word on I44).
- The quicksave's size (I45) and the modding layers (I33).

## 10. Not verified, and the measurements missing

- That the 104 x87 and 23 control-word blocks assemble under the `x86_64`
  driver as written: the classifier read mnemonics, not operand
  constraints; XA3 builds them.
- That Windows loads a PE32+ with no relocations at `0x400000`, and a
  64-bit DLL at a low fixed base: XA2's probes. The low-memory probe was
  not re-run on 2026-10-31.
- Which of the 29 live allocation sites hand an address to the game: XA2
  reads them.
- Where the 64-bit `dsound` puts its objects, and whether a DirectSound shim
  or SDL3 audio is the smaller change: XA2.
- The 64-bit frames' depth on the task stacks: a stackwatch run on the
  64-bit host, stage C.
- That Windows ships a 64-bit Cinepak decoder reachable through MCI: stage
  C's run 3.
- The pace cost of `Img()` as a native pointer with a run-time base on
  x86-64: stage C's run 5.
- Whether anything in `src/game` depends on `__stdcall` / `__cdecl` beyond
  the spelling (the aliases of section 0): XA1 lists, XA3 compiles.
- Which naked functions exist only for a Capcom caller's register contract:
  XA3.
- The 2026-10-29 figures that do not reproduce with a recorded command (the
  asm split 125 / 29 / 20, "40 naked in 16 live files", "39 pointer fields",
  "19 narrowings of a function"): replaced by section 0's commands; XA1's
  tool is the one count from then on.
