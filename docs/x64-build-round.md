# The 64-bit build round: staging (PLAN phase 4, I44 "What follows")

**Status:** IN PROGRESS (stage B complete and gated 2026-11-02 on `x64/round`, PR at the owner's word; stage C running. Launched 2026-11-01 on `x64/round` off `main`
`7bd917de`, the PR #176 merge; the owner's four calls answered the same
morning, section 7; stage A's four groups run in worktrees under
`C:/Users/kerok/bof3x-runs/x641101`. Staged 2026-10-29, re-measured
2026-10-31 on `main` `43e98773`. The relocatable-image round it waited on is
merged (PR #168, with the state hash's pool fold as PR #170); section 0.1
lists what that merge changed in this plan.)

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
| Inline asm in live code (`src/`, fuzz files apart) | **174 blocks in 70 files** (the same total as 2026-10-29; re-measured 2026-11-01 on `21d06502` by `width_census.py`, the same). By class: **104 x87 arithmetic** in 36 files (32 hold nothing else; `d3d_draw` 8, `d3d_rest` 8, `effect_1a` 7, `effect_3c` 7, `effect_3d` 7); **30 stack or control flow** in 15 files (`task_sched` 6, `sound_rest` 5, two each in `field_misc`, `mode_tasks`, `save_menu`, `scena_calls`, `shell`, `stackwatch`, one each in `mp3.cpp`, `area_slope`, `battle_misc`, `field_e2`, `field_modes`, `magic_s28`, `map_field_objects`); **23 control or status word only** in 11 files; **6 `fs:` reads** in 3 files (`boss_harness` 4, `scenario_harness` 1, `ImageBase()` 1); **11 other** in 10 files (**6** `cltd; idivl` - the tool and XA3's build both count 6, the "7" of 2026-10-31 was a hand count - 2 byte loads, `GlyphTrap`'s privileged `in` at `text_draw.cpp:44`, `gapwatch`'s load, `rest_1e.cpp:180`'s empty register barrier). 6 asm-label aliases (`__asm__("_Music_Play")` and five more, `sound.cpp` 5, `sound_rest.cpp` 1); 120 more blocks in 63 fuzz files, 6 more aliases in 2 | `python tools/width_census.py --counts` (`asm_live`, `asm_<class>_live`, `asm_live_x87_only_files`, `asm_alias_live`; the `asm` rows of `analysis/width_census.tsv`). A block is **stack** if its text has `push`, `pop`, `call`, `jmp`, `ret`, a `j` branch, `esp` or `ebp`; else **fs** if it reads `fs:`; else **x87** if any mnemonic is an x87 operation other than `fnstcw` / `fldcw` / `fnstsw`; else **control word** if every mnemonic is `fnstcw`, `fldcw`, `stmxcsr` or `ldmxcsr`; else **other**. The 2026-10-29 split (125 / 29 / 20) used a classifier that was not kept, so its classes differ from these; the tool's classifier reads the template strings of the blanked source, so a mnemonic in a comment is not a block's |
| Asm that names a C symbol by its x86 spelling | **29 operands in 10 live files** name a symbol with the leading underscore of the i686 ABI (`call _Task_SetStackBase`, `jmp *(_SoundRest_g+8)`), which x86-64 does not prefix: 19 of other symbols in 8 files and **10 that load `_Bof3_ImageMove`** by absolute address (`task_sched.cpp` 8, `field_e2.cpp:126`, `field_modes.cpp:140`; the variable at `host.cpp:25`), which x86-64 cannot encode outside the low 2 GB without a RIP-relative form. The 2026-10-31 "39 in 16 files" was a `grep` over whole lines: it counted 11 `call _ftol` in comments and missed `cmpb $0, _StackWatch_Armed` (XA3's build agrees: 29) | `python tools/width_census.py --counts` (`asm_symbol_live`, `asm_symbol_underscore_live`, `asm_symbol_imagemove_live`); `grep -rn "_Bof3_ImageMove" src` for the 10 |
| Naked functions | **69 in 31 files**: 42 in 18 fuzz files, **27 in 13 live files** (`task_sched` 6, `sound_rest` 5, two each in `shell`, `scena_calls`, `save_menu`, `mode_tasks`, `field_misc`, one each in `map_field_objects`, `magic_s28`, `field_modes`, `field_e2`, `battle_misc`, `area_slope`). The 2026-10-29 "40 in 16 live files" does not reproduce on its own tree with this command (27 in 13 on `628aa521`); the census replaces it | `python tools/width_census.py --counts` (`naked`, `naked_live`, `naked_live_files`, `naked_fuzz`); the same as `grep -rlE "__attribute__\(\(([a-z_, ]*)naked" src` counted per file |
| Calling-convention spellings | `__cdecl` 16,156, `__stdcall` 302, `WINAPI` 24, `CALLBACK` 17 (the 2026-10-31 `grep` counted comments too: 16,160 / 305 / 24 / 19); no `__fastcall`, `__thiscall` or `regparm`. All are ignored on x86-64; what depends on them is the asm above | `python tools/width_census.py --counts` (`callconv_*`, outside comments and strings) |
| A native address stored into a 32-bit slot | **801 sites, 453 in 272 live files, 348 in 247 fuzz files** - the 2026-10-31 "384" counted the `static_cast<std::uint32_t>(...)` spelling alone; the tool reads every outer type (`U` 343, `unsigned long` 32, `unsigned` 6, `uint32_t` 2, `DWORD` 1, `grep -rhoE "static_cast<T>\(reinterpret_cast<std::uintptr_t>\("` per `T`) and the 29 `reinterpret_cast<U>(p)` integer casts of a pointer (`rest_1c.cpp` 28, a compile error on x86-64). **160 of the 453 live sites are one-line helpers** (`Key(const void* p)`, `Address`, `Addr`, `Va`, `AsU32`: `bind.cpp:22`'s `Key()` narrows every function of ours that the bind table jumps to) whose **1,170 live calls in 155 files** the tool lists and classifies too. By what is narrowed, live, sites plus helper calls: **image** 26 + 513 (a typed name, `Img(...)`, a view), **a local record pointer in a game file** 81 + 519 (by rule, not by eye: section 3 "XA1 as run"), **an object of ours** 78 + 64 (`&g_x` switch keys, static tables, buffers), **a function of ours** 36 + 41 (`callee`, `entry`, `&Menu_DrawFrame`, `__builtin_return_address`, the bind table's `.ours`), **the draw pool** 18 + 13, **Windows'** 16 + 9 (`BaseAddress`, `ExceptionAddress`, the TEB, a module, `&GetTickCount`, DirectInput's descriptors), the C run time 1 + 1, the heap 0 + 1, **unread 9 + 9** (listed by name in section 3 for B2's eye) | `python tools/width_census.py --counts` (`narrow_*`, `narrow_call_*`); the `narrow` and `narrow_call` rows of `analysis/width_census.tsv`, `detail` the sub-class, `token` the argument |
| A 32-bit value used as a native address | **960 sites, 650 in 289 live files** (the 2026-10-31 "950" was the `static_cast<std::uintptr_t>` spelling alone; the tool adds the 22 `reinterpret_cast<T>(::bof3::Img(va))`, Img returning the 32-bit word today). By the word, live: 261 a bare parameter or member (`address`, `a`, `va`), 181 a word read out of the image (`Long(Mem(...))`, `UL(At(...))`), 76 `::bof3::Img(...)`, 25 `::bof3::Code(...)` / `CodeToCall` / `CodePlaceToCall`, 107 other; **229 of the 650 cast to a function-pointer type** (`Handler`, `HookFn`, `R (__cdecl*)(...)`: a code word called). 1,811 spellings of `std::uintptr_t`, 1,145 of them live; `sizeof(void*)` twice (`wav_dump.cpp:495`, `:498`) | `python tools/width_census.py --counts` (`widen_*`, `widen_live_fnptr`, `uintptr`, `sizeof_ptr`); the `widen` rows, `type` the target type |
| `Img()` places | **14,960** (14,977 by the `grep`, which counts comments; 1,070 of them in fuzz files); `VaOf()` 50 (unchanged); `Code(` 605 spellings (617 by the `grep`). `Img`, `VaOf` and `ImageBase` take and return `std::uint32_t` (`src/common/image_base.h:60`, `:69`, `:73`); `ImageBase()` reads the PEB's `ImageBaseAddress` through `fs:0x30` + 8 (`image_base.h:60-66`), an x86 layout (x86-64: `gs:0x60` + 0x10) | `python tools/width_census.py --counts` (`img_place_Img`, `img_place_VaOf`, `img_place_Code`, outside comments and strings) |
| Code words and calls through an address | 8,784 `.text` words, bound by name since stage C2; the bind table has 10,088 rows; `gate_abs_call` 1,340 run-time calls through an address, `call_through_data_word` 745 | `python tools/reloc_census.py --counts`; `symbols.toml` (10,088 `impl` of 10,100 `[[func]]`) |
| The bind table's rows | each row is a 5-byte `jmp rel32` at `(top << 24) \| VA` in a table the process reserves under a measured top byte 0x7C, 0x70, 0x7E, 0x7B or 0x7D, then any 0x20..0x7F (`bind.cpp:72`, `BuildTable` at `:98`); rows are 11 bytes apart at the nearest. A 64-bit absolute jump needs 12 bytes or more, so the 5-byte row survives only if our code lies within 2 GB of the table | `src/hook/bind.cpp` |
| Where our code lies | `bof3x.dll` linked at `0x10000000` with `--disable-dynamicbase` (`CMakeLists.txt:768`); `bof3x.exe` is not linked but written by `tools/make_exe.py` as a PE32 at `0x400000` (magic `0x10B`, machine `0x14C`, `make_exe.py:389`, `:400`) whose sections are the image, importing `bof3x.dll!Engine_Main`. One `bof3x.dll` serves both hosts (`image_base.h` header comment) | the files |
| The import slots | **14 four-byte slots** in the image that the exe host fills with a system DLL's function (`exe_host.cpp:407` `kSlots`; `BindSlots` at `:435` copies 4 bytes, `:438`): DSOUND ordinal 1, three GDI32, six KERNEL32, four USER32. In a 64-bit process those functions lie above 4 GB | the file; `python tools/width_census.py --counts` (`import_slot`, read from `kSlots`) |
| The unsigned longs | 1,304 spellings of `unsigned long` in `src/game` outside comments (1,333 by the `grep`, which counts comments), 1,325 live in `src/`, 1,485 in all; `long` on its own 3,010 live (mostly Capcom's signatures: `long*`, `long (__cdecl*)(long, long)`), 3,588 in all; 6,861 rows in the census. The generated ctypes: 1,745 `unsigned long`, 42 `long`, 1 `unsigned long *` of 3,165 `[[data]]` ctypes (3,249 rows) | `python tools/width_census.py --counts` (`long_src_game_ul`, `long_ul_live`, `long_l_live`, `long_ctype`); the `long` rows, `detail` `ul` / `l` / `ctype:<ctype>` |
| The records | 43 structs in `src/game/records*.h` (23 + 8 + 12), each sized by a `static_assert` (43); **no pointer member**: "Address fields are std::uint32_t: the image is 32-bit whatever the host" (`records.h:4`); at least 9 `std::uint32_t` members are documented as an address or a pointer by their comment. The 2026-10-29 count of 39 such fields came from a wider read, not re-measured | `grep -hcE "^\s*struct " src/game/records*.h`; `grep` for a `T *` member (none) |
| The data image as a struct | `GameData` is `0x37B000` bytes (`static_assert` in the generated header), **2,968 named members** (2,965 before) and 871 pads. **Corrected 2026-11-01 by XA3's build: 109 members are pointer-typed** (`using X_type = void *;` 73, `unsigned char *` 26, `const unsigned char *` 4, `const char *` 2, `unsigned long *` 1, three `records::* *`), generated from the **111 `[[data]]` rows whose ctype holds a `*`**, so `sizeof(GameData)` is 3,654,176 on x86-64 against 3,649,536 and no game file compiles until `tools/gen_symbols.py` emits a 32-bit slot for a pointer ctype - stage B3's first unit. The brace-depth parse of 2026-10-31 read the members' names, not their `_type` aliases. By what the slot holds (the census's `ptr_member` rows): 42 a pointer into the image, 26 a table of code words, 14 the import slots' typed names, **10 a COM object of Windows'** (`Snd_Device`, `Snd_Primary`, `Music_Buffer`, `Music_Notify`, `SndStream_Buffer`; `DInput_Object`, `DInput_Keyboard`, `DInput_Joystick`, `DInput_Joystick2`; `Fmv_DDraw`), 3 a handle (`Music_Events`, `Game_Hwnd`, `Game_HInstance`), **11 the render shim's objects**, 3 a `Crt_malloc` block (`Music_Data`, `Music_File`, `Font_GlyphData`), 1 the MP3 decoder's handle (`Music_Decoder`), 1 the C run time's `FILE`s (`File_Slots`) | `grep -cE "^ +using [A-Za-z_0-9]+_type = .*[*];" build/gen/bof3/symbols.gen.h` (XA3); `python tools/width_census.py --counts` (`ptr_member`, `ptr_member_<holder>`) |
| The typed-data census | 27,427 raw data sites, 9,059 record sites, 17,239 global sites, 8,916 in fuzz files | `python tools/data_census.py --counts` |
| Allocation in `src/game` | **47 sites, unchanged**: 20 `malloc`, 8 `Crt_malloc`, 14 `VirtualAlloc`, 3 `Crt_calloc`, 2 `calloc`; **29 of them in live files** (20, 6, 1, 1, 1); 8 more in `src/hook`, `src/render`, `src/audio`, `src/common` (1 `malloc`, 7 `VirtualAlloc`) | `grep -rhoE "(^\|[^A-Za-z_])malloc\("` and the same per name, live and all |
| The task stacks | four slices of `0x4000` on the process's main thread stack; `Task_StackTop` a 32-bit slot (`symbols.toml:803`) and each record's saved `esp` a 32-bit word; the switch is naked asm (`task_sched.cpp` 1..26); four slices need `0x14294` bytes below base on the 32-bit build | `symbols.toml:803`, [`quicksave.md`](quicksave.md) 1, [`state3-design.md`](state3-design.md) 5 |
| Other stacks | the present runs on a fiber with a 1 MB stack (`render_shim.h` header comment; `ConvertThreadToFiber` 3, `CreateFiber` 4); `mp3.cpp`'s decoder switches `esp` in asm | `grep -rhoE "ConvertThreadToFiber\|CreateFiber" src` |
| Where the packets may lie | below 16 MB: the ordering table's links are 24-bit (`d3d_list.cpp:209-210`), DIV-0062's pool asserts it, and `host.cpp:54-58` refuses an image that ends past `0x1000000`. The legal moved bases are `0x990000..0xA70000` | [`relocatable-image-design.md`](relocatable-image-design.md) 1.2, [`relocatable-image-round.md`](relocatable-image-round.md) 4 |
| The image span | `.data` at `0x5C4000`, `0x37B000` bytes; `.bss` zeroed to `0x93D6EC` | [`relocatable-image-round.md`](relocatable-image-round.md) 0 |
| The Direct3D and DirectDraw objects | **ours**: `render_shim.cpp` builds DirectX 6-shaped objects with x86 layouts (`DDSURFACEDESC2` as `0x7C` bytes, every method `__stdcall` "as COM is on x86", `render_shim.cpp:1-4`) and the game's draw code calls them through their vtables; `IDirectDraw*` named 24 times in 10 live files, `IDirect3D*` 36 times in 16 | `grep -rhoE "IDirectDraw[A-Za-z0-9]*\|DirectDrawCreate[A-Za-z]*"` (and `IDirect3D`) over the live files |
| The DirectSound objects | **Windows' own**: `DirectSoundCreate` through the import slot `0x5C4014` (`exe_host.cpp:408`); the device, primary, music stream, notify and SND stream buffers are kept in 32-bit slots of the image (`Snd_Device`, `Music_Buffer` and the rest, `symbols.toml:8889-8930`; the census's `ptr_member` rows of detail `windows_object`, which also hold **the four DirectInput objects** `pad_read.cpp` makes through `dinput.dll` and the FMV's own DirectDraw `Fmv_DDraw` - Windows' too, not in the 2026-10-31 read); 54 vtable calls through `Method<>()` in 5 sound files (`sound.cpp` 32, `sound_rest.cpp` 9, `battle_items.cpp` 7, `sound_sp.cpp` 5, `sound_callees.h` 1); `Snd_BufferDesc` is the 32-bit `DSBUFFERDESC` (nine words, `dwSize` `0x24`, `symbols.toml` `Snd_Init`'s evidence), which on x86-64 holds a pointer and is larger | `grep -rhoE "\bMethod<?[^(]*\(" src/game`, counted per file; `python tools/width_census.py --list ptr_member:windows_object` |
| The rest of the Windows surface | `d3d11` included in 2 files (`src/render`), `dinput` in 4 (3 `src/game`, 1 `src/input`; `pad_read.cpp:241` loads `dinput.dll`), `mmsystem` in 2 (`src/hook` for the FMV's MCI, DIV-0035; the launcher); the DLL links `d3d11 dxgi d3dcompiler_47 uuid winmm bcrypt ole32 windowscodecs dinput dxguid SDL3::SDL3-static` (`CMakeLists.txt:758`); **SDL3 3.4.16 already fetched** for the pad (DIV-0050, `CMakeLists.txt:93-112`, [`THIRD_PARTY.md`](THIRD_PARTY.md) 2) | `grep -rlE "#include\s*[<\"]<name>(\.h)?[>\"]" src` per header |
| C++ floating point | already SSE in the `i686` build: the DLL's code has 9,551 `movss`, 1,129 `mulss`, 893 `addss` against 557 `flds` and 994 `fstps` (the asm blocks, and the i686 ABI's float returns in `st(0)`). So the x86-64 build's SSE arithmetic is the 32-bit build's, outside the asm | `llvm-objdump -d --no-show-raw-insn build/bof3x.dll`, mnemonics counted (the build of 2026-10-30) |
| The log formats that print a place | 434 conversions (`%08X` 367, `%p` 67 by the `grep`), **366 in live files**: 78 print a pointer whole (`%p`, `(void*)x`: 16 digits on x86-64 unless the design keeps the place low; `wav_dump.cpp` 17, `detour.cpp` 10, `snapshot.cpp` 9, `sound.cpp` 8, `exe_host.cpp` 7), 19 print a narrowing of their own (`(unsigned)reinterpret_cast<std::uintptr_t>(p)`, `Address(sprite)`: B2's helper's), 269 print a word (a VA, a count, a flag) | `python tools/width_census.py --counts` (`fmt_place_*`); the `fmt_place` rows, `token` the argument each conversion prints |
| The FMV codec | `capcom.avi` shipped as Indeo 5, which no 64-bit Windows decodes; already re-encoded to Cinepak by the ledger's entry "Re-encode `capcom.avi` from Indeo 5 to Cinepak" ([`DIVERGENCE.md`](DIVERGENCE.md) line 279) - **wrong, 2026-11-07: Windows has no 64-bit Cinepak decompressor either** (`iccvid.dll` is in `SysWOW64` alone), D407 | the ledger |
| The quicksave's build check | the quicksave's `BuildId` holds the host, the version tag, the **sha256 of `bof3x.dll`** and the DLL's base as a 32-bit word (`snapshot.cpp:126-131`, filled at `:300-319`). A quicksave from another DLL is refused already, so a 32-bit take is refused by a 64-bit build without new code; `dll_base` is the one field that must widen or stay low | the file |
| The state hash | the tool reads `BOF3SH1`..`BOF3SH5` (`tools/statehash.py:34-75`); since PR #170 a folded `BOF3SH5` run refuses a `BOF3SH4` folded file by design; the hash maps a code word back to its VA only inside the bind table (`bind.cpp` header comment) | [`state-hash.md`](state-hash.md) 8, [`statehash-pool-fold.md`](statehash-pool-fold.md) |
| **The low-memory probe** (a 64-bit program built with the `x86_64` driver; **not re-run** on 2026-10-31) | 2026-10-29: `VirtualAlloc(0x400000, 0x600000)` succeeds; the window `0x10000..0x1000000` is entirely free before our block; 64 MB at `0x1000000` succeeds. The same program linked by lld with `--image-base 0x400000` was moved by ASLR to `0x4E0000` and the block then failed (error 487). Not measured: a PE32+ that `make_exe.py` writes with no relocations and no `DYNAMIC_BASE`, as `bof3x.exe` is today | `tools/probes/lowmem_probe.cpp` |

Three consequences:

- **The game's words never need to widen.** No record holds a pointer; a
  pointer field is a 32-bit VA slot already. (`GameData` does, 109 members
  by the generated `_type` aliases - the correction above - but each is a
  4-byte slot of the image spelled with a pointer ctype, and B3's first unit
  makes `gen_symbols.py` emit it as the 32-bit slot it is.) What
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

### XA1 as run (2026-11-01, branch `x64/xa1`, report `x641101\xa1\REPORT.md`)

`tools/width_census.py` (in `reloc_census.py`'s shape: `data_census.py`'s
reader, so a spelling in a comment or a string is not a site; `--counts`,
`--check`, `--list CLASS[:DETAIL]`) and its committed output
`analysis/width_census.tsv` (14,481 `site` rows - every class, fuzz rows
included - then `file` rows and `#count` lines; 2.2 MB, listed in
`.gitignore` beside the other censuses). `checks.yml` runs `--check`: a
live site keyed by (class, sub-class, file, token, the line's text) that
the committed file does not list fails CI, `reloc_bare.py`'s rule; the
negative control (a probe file with one narrowing and one `pushl` block)
was refused with three rows named, then removed. Section 0's table now
cites the tool wherever it reproduces a count; where the tool and the
2026-10-31 `grep` differ, the row says which is right and why - every
difference is a `grep` counting comments or one spelling of several.

What the stage B groups take from the TSV:

- **B2 (the narrowings)**: the `narrow` rows (453 live) and the
  `narrow_call` rows (1,170 live calls of the 160 one-line helpers), each
  with the sub-class in `detail`. Trust it this far: `windows`, `func_ours`,
  `pool`, `crt`, `heap` and `object_ours` are read from the argument's
  spelling (a Windows field name, a function defined in `src/`, `&g_x`); `image`
  from a typed name or a placer; **`image_local` (81 + 519 live) is a rule,
  not a reading**: in a game file a bare local, an offset of one, a record
  helper's result (`Party(who)`, `Cur() + 0x2E`) or a view (`rv::Bytes(X)`)
  is taken to point into the image, because an object of ours in a game
  file is `g_`-named or `&`-taken. B2's `Low32()` trap on the 64-bit build
  is what proves the rule; a site it fires on is a mis-classed row. The
  **18 unread live rows** are for B2's eye: `mp3.cpp:285` (`data`),
  `crash.cpp:262` (`seconds`), `crash.cpp:273` and `host.cpp:31` (`base`),
  `stallwatch.cpp:123` (`m`), `gfx_snapshot.cpp:33` (`p`),
  `render_shim.cpp:629` (`s->pixels`), `:697` (`self`), `:945` (`texture`),
  `battle_draw.cpp:385`, `display_env.cpp:272`, `rest_2f.cpp:1142`,
  `save_menu.cpp:544` (a draw helper's returned pointer), `tex_page.cpp:291`
  (`*surface`), `gapwatch.cpp:467`, `:468`, `:558`, `:617` (`m`, `base`,
  `s`). The `widen` rows (650 live) carry the target type and whether it is
  a function pointer (229), and the word's shape; the `fmt_place` rows the
  78 `%p`-style prints.
- **B1 (the asm)**: the `asm` rows by class with the mnemonic set, the
  `asm_symbol` rows (29 live: 19 underscored names, 10 `_Bof3_ImageMove`),
  the `asm_alias` rows (6), the `naked` rows (27 live) and the `fs_read`
  rows (6). XA3's build (`x64-asm-policy.md`) is the verdict per block;
  the census is the list.
- **B3 (the types)**: the `ptr_member` rows first - the 111 `symbols.toml`
  rows whose ctype holds a `*`, each with what the slot holds - then the
  `long` rows (1,325 `unsigned long` and 3,010 `long` live in `src/`, 1,788
  generated ctypes).
- **B4 and B5**: the `import_slot` rows (14), the `ptr_member` rows of
  detail `windows_object` (10), `windows_handle` (3), `ours_render` (11),
  `ours_decoder`, `ours_crt`, `heap`; the `valloc_addr` rows (5 live:
  `draw_pool.cpp:280`, `bind.cpp:90`, `exe_host.cpp:193`, the launcher's
  two); the `sizeof_ptr` rows (2).

Not verified by XA1: the sub-classes were not checked against a run; the
`long` rows are marked, not judged (Win64 keeps them 32 bits); the generated
header was not rebuilt here (XA3's 109 is cited, the 111 rows are the
tool's); `--check` was run on this machine, not in CI.

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

### XA2 as run (2026-11-01, branch `x64/xa2`, report `x641101\xa2\REPORT.md`)

[`x64-build-design.md`](x64-build-design.md) and four probes under
`tools/probes/` (`lowmem_probe.cpp` extended, `lowdll_probe.cpp`,
`taskstack_probe.cpp`, `frame_growth.py`), measured on this machine. The
decisions the probes forced: option (a), `make_exe.py` writes a PE32+ at
`0x400000` with no `.reloc` and no `DYNAMIC_BASE`, which Windows 11 loads
there (the 2026-10-29 "moved to `0x4E0000`" was that build's `DYNAMIC_BASE`
bit); with it off the loader places the whole process bottom-up below 4 GB
(the main stack, the heap, `VirtualAlloc(nullptr)`, the fiber stack, the
Direct3D 11 device, the SDL3 stream), so **no low arena**: the OS's placement,
`Low32()` as the trap and a check in `Crt_malloc` (the one chokepoint of the
9 allocation sites whose address reaches the game), a `HeapCreate`d low heap
as the fallback. The window below 16 MB is *not* free in such a process (an
anonymous 16 MB view sits at `0x1000000`), which option (a) never needs. A
64-bit `bof3x.dll` at `0x10000000` is honoured and a 5-byte `jmp rel32` row
reaches it under every top byte the bind table may take (no 14-byte row); the
14 import slots get a 14-byte `jmp [rip+0]` thunk each in one extra page of
the table's reservation. The task slices stay on the main thread's stack with
an `rsp` switch (8 GPRs and `xmm6..xmm15`, no x87 or MXCSR state per task:
38 control-word blocks hold no call), a per-architecture `task_switch_<arch>.h`
contract for the owner's call 2; frames grow 2.18x over 722 matched
functions, `kSlice64` estimated `0x8000` for stage C's stackwatch to set. The
sound objects become ours with DirectSound-shaped vtables over one SDL3
stream and a mixer (design 6.3; the x64 `DSBUFFERDESC` is 40 bytes against
the game's `0x24`, so a shim over the 64-bit `dsound` would marshal 11 of 21
methods); the 54 `Method<>` sites stay as they are. `Img()` stays 32-bit,
`ImageBase()` by width, the quicksave's format unchanged. Section 9 of the
design is `Slot32<T>` for the 111 pointer-typed `[[data]]` rows.

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

### XA3 as run (2026-11-01, branch `x64/xa3`, report `x641101\xa3\REPORT.md`)

[`x64-asm-policy.md`](x64-asm-policy.md): every one of the 69 live files
with asm compiled with its own i686 command from `compile_commands.json`
and the driver swapped. **All 127 x87 and control-word blocks assemble
unchanged and are right**; they stay asm. 15 of the 30 stack blocks fail
(`push` / `pop` of a 32-bit register, `jmp *%e..`); of the 15 that assemble,
12 are wrong on x86-64 (`4(%esp)` arguments, a 32-bit `esp`, a struct offset
that doubles, `_Bof3_ImageMove` as `ADDR32`) and 3 right. The 6 `fs:` reads
assemble and read the wrong segment (the TEB is at `gs:`). The 11 "other"
blocks are right, the privileged `in` included. 24 of the 27 live naked
functions are reachable in the exe host; every caller is ours in both hosts.
The spellings: 29 real underscored operands in 10 files (the 39 counted
comment lines), the 6 aliases and the 10 `_Bof3_ImageMove` loads, spelled
on x86-64 by a `__USER_LABEL_PREFIX__` macro and `Bof3_ImageMove(%rip)`.
The build found the wider defect first: **109 `GameData` members are
pointer-typed** in the generated header (111 `[[data]]` rows with a `*`
ctype), so `sizeof(GameData)` is 4,640 bytes too large on x86-64 and no game
file compiles until the slot type lands; the asm verdicts were taken
against a patched scratch header. Section 8 of the doc is B1's list in ten
units, unit 0 the slot type.

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

### XA4 as run (2026-11-01, branch `x64/xa4`, report `x641101\xa4\REPORT.md`)

`cmake/x86_64-llvm-mingw.cmake`, the preset `x86_64` (build dir
`build-x86_64`), the guard a choice (`BOF3X_WIDTH` 32 / 64,
`BOF3X_HOSTED_BUILD`); the 64-bit DLL is the same source list minus the 253
`*_fuzz.cpp` (632 against 379 objects), `bof3x-launcher-dev.exe` i686 only,
the hosted mode's own files in both widths because 340 live objects reference
`detour.cpp` (their `BOF3.exe`-only paths are B4's `#if`); a target
`bof3x_compile` (every object, no link) for CI. `make_exe.py --width 64`
writes a PE32+ (`0x20B`, `0x8664`, 8-byte thunks, an amd64 manifest, an
entry with RIP-relative IAT calls), `pe_read.py` reads both widths; the
32-bit output byte-identical to before. SDL3 3.4.16 builds for x86-64
unchanged. `build.yml`'s job `x86_64`: configure blocking, compile and link
as `continue-on-error` steps until stage C, the artefact
`bof3x-x86_64-<sha>`. The i686 build: `codegen_same.py` against a baseline
build of `21d06502`, `648 objects compared (code and data), 0 differ`. The
local `x86_64` preset configures; 352 of 631 objects fail, 349 of them at
the generated header (XA3's finding), 13 on their own front-end errors (the
x86 `CONTEXT` users `crash`, `gapwatch`, `stackwatch`, `stallwatch`,
`calltrace`, `win_main`; `fmv_play.cpp` `GWL_WNDPROC`; `rest_1c.cpp:460`'s
28 pointer-to-integer casts; `rdata_consts`'s DirectInput x86 layouts,
`gfx_clut`, `gfx_texcache`, `sound_rest_callees.h:52`). A pointer ctype as a
plain `std::uint32_t` member was tried and reverted: 445 of 891 i686 objects
then fail on 4,380 sites that use the pointer type, so the slot wants a
conversion type (the design's `Slot32<T>`), B3's first unit.

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

### Stage B as launched (2026-11-01, off `x64/round` after the four stage A merges)

Two waves, by file overlap. **Wave 1, four groups in parallel**, worktrees
under `x641101\b<n>`, merged in the order B3, B1, B4, B5:

- **B3** owns `tools/gen_symbols.py` and the generated header: `Slot32<T>`
  for the 111 pointer ctypes (design 9) and the 53 address-of sites, `0
  differ` on i686; then the `x86_64` compile of `src/game` is measured again
  (349 objects should pass the header). The `unsigned long` rows are wave 2's.
- **B1** owns the asm policy's units 1..9 and `stackwatch.cpp` whole (unit
  10's `:242` and `CONTEXT` lines included, so one owner per file) and the
  one-line `rsp` check in `Task_SetStackBase` (design 7.1), since
  `task_sched.cpp` is its; the gates per unit as the policy's section 8
  table names them, the hosted fuzz run headless by the group in its own
  game copy.
- **B4** owns `image_base.h` (with `Low32()` as design 8.1 writes it, which
  wave 2's B2 then uses everywhere), `exe_host.cpp`, `bind.cpp`, `host.cpp`,
  `snapshot.cpp`, `crash.cpp` (design 11), `crt_rest.cpp`'s `Crt_malloc` /
  `Crt_calloc` check (design 5), the other `CONTEXT` users (`gapwatch`,
  `stallwatch`, `calltrace`, `win_main`), `fmv_play.cpp`'s `GWL_WNDPROC`; `0
  differ` on i686 or the fuzz.
- **B5** owns `src/audio/sound_sdl.cpp` (design 6.3) and the WAV dump's tap,
  reserves DIV-0201 (the sound output through SDL3 on both widths, the
  32-bit build keeping DirectSound behind a switch for the owner's A/B
  listen), and touches `exe_host.cpp` for the slot binding alone (merged
  after B4, keep-both).

**Wave 2** after the four merge: **B2**, the 453 live narrowings through
`Low32()`, `Key()` and `Addr()` with it, the `%p` prints, `rest_1c.cpp:460`'s
28 casts, the `rdata_consts` / `gfx_clut` / `gfx_texcache` asserts by width,
and the `unsigned long` rows; `0 differ`. Then stage B's PR.

### Stage B wave 1 as run (2026-11-01, merged on `x64/round` in the order B3, B4, B5, B1; reports `x641101\b<n>\REPORT.md`)

The four groups ran in parallel, were paused once at the account's usage
limit (each committed work in progress and a state report, and resumed with
its context), and merged the same evening. The round branch moved to its own
worktree (`x641101\round`) when a concurrent session on the main checkout
switched branches under the first two merges; those were reset off its branch
and redone. The same session took DIV-0197 and DIV-0198 for issues #177 and
#178, so B5's entry is **DIV-0201**, and the round's PR follows theirs so the
ledger has no gap (the ledger's status line and `ledger_registry.h` are
settled when `main` is merged in).

- **B3** (`6710e1c9`): `src/common/slot32.h`, one `Slot32<T>` struct on both
  widths (the coordinator's call: no `using Slot32 = T*` on i686), 4 bytes,
  `T* v` behind a reference conversion on i686, `std::uint32_t v` with a
  below-4 GB `Fatal` on x86-64; `gen_symbols.py` emits it for every pointer
  ctype (220 generated lines); 58 sites spell `.Native()`, 48 of the 53
  address-of sites unchanged, the three sound out-slots
  `reinterpret_cast<void**>(&slot)` (B5's `Create` writes 4 bytes, so they
  match), `rest_2h`'s DirectInput out-parameters as native locals stored
  back. `codegen_same.py`: 648 objects, 4 differ, each proved (`rest_2h` the
  intended locals, its fuzz 144,000 rounds 0 mismatches; `rest_2h_fuzz` the
  fakes; `magic_s13` mangled names only, 50,000 rounds; `sound.cpp` one
  six-instruction hunk in `sound_snapshot::SelfTest`'s own check chain). The
  hosted suite `'*'` and the exe host passed. The `x86_64` compile: **20 of
  631 objects fail, none at the generated header** (XA4 saw 352).
- **B4** (`9e474043`): `image_base.h` by width (`ImageBase()` from
  `gs:0x60`+`0x10`, `Low32()` **a macro that is exactly the cast on i686**
  and a checked always-inline function on x86-64, the two-signature shape so
  `__builtin_FUNCTION()` defaults never reach i686 `.rdata`; `LowBlock()`),
  `host.cpp` (the PE32+ check, the `BOF3.exe` identity refused on x86-64,
  `Bof3_ImageMove` 32-bit), `bind.h` / `bind.cpp` (`Key()` through `Low32`,
  the thunk page, `Bind_ImportThunk`, a rel32 reach check on x86-64),
  `exe_host.cpp` (`BindSlots` thunks), `snapshot.cpp` (`kSlice`
  `0x4000` / `0x8000`, `dll_base` 32-bit), `crash.cpp` by design 11, a new
  `hook/cpu_context.h` for the `CONTEXT` users, `fmv_play.cpp` `GWLP_WNDPROC`,
  `crt_rest.cpp`'s `Crt_malloc` / `Crt_calloc` through `LowBlock` (no
  `HeapCreate` fallback: `_msize` in `sound.cpp` would break), `detour.cpp`'s
  hosted writers a `Fatal` on x86-64, `crash_report.py` and `task_stacks.py`
  by width. **648 objects, 0 differ**; the hosted suite, the exe host and the
  moved base passed; all 16 owned files compile for x86-64 against B3's
  header. The launcher's fence under option (a) read correct as it is.
- **B5** (`54b5714e`, renumbered `da5b773b`): `src/audio/sound_sdl.{h,cpp}`,
  objects of ours with DirectSound-shaped vtables (the device, a 1,024-buffer
  static pool, each buffer's notify facet, a mixer, one SDL3 device stream),
  bound on the `DirectSoundCreate` slot by `SoundSdl_Start` from `dllmain.cpp`
  and `exe_host.cpp`; the switch `SoundSdl` a `PatchBytes` on the slot,
  ignored with a log line on x86-64; the WAV dump wraps either `Create`;
  `SDL_AUDIO` turned on in `CMakeLists.txt`. 7 differ, all its own or the
  registry's; the hosted suite with the switch on and under
  `BOF3X_OFF=SoundSdl`, the exe host on and off, all passed; the SDL3 device
  opened in the exe host's self-test (Speakers, 44,100 Hz, 441 frames). The
  hosted self-test skips the device open: under `DllMain`'s loader lock it
  deadlocked (in play `Create` runs from `Snd_Init` on the game thread).
  **Nobody has listened**: the owner's list is in DIV-0201's verification
  field (a loop point, a battle, DIV-0171's pans and fades, the inn jingle,
  the quicksave take and load, the same route under `off=SoundSdl`). The
  WAV pair shows what the game asked of either layer, not the mixer's output.
- **B1** (`3d843a17`, ten commits): unit 1 `src/common/asm_name.h` (the
  `__USER_LABEL_PREFIX__` macro, the six aliases, `MoveCmd_TestFB/FC`,
  `shell.cpp`'s `SHELL_SYM` folded); unit 2 `task_switch.h` with
  `task_switch_i686.h` / `task_switch_x86_64.h` (the i686 text byte for byte,
  the x86-64 switch for `rsp` with 8 GPRs and `xmm6..xmm15`, the contract a
  third variant joins), `stackwatch.cpp` whole by width; the mp3 decoder's
  stack switch as a pair; units 3..6 all **pairs** (the call tables with
  `Bof3_ImageMove` sign-extended by `movslq`, the ecx entries, the sound jumps,
  `Battle_StatusTint` and `ScriptFlags_Clear40` - a thunk failed
  `Battle_StatusTint`'s fuzz 1,494 of 3,000 rounds because the fuzz compares
  the whole `eax`, so the pairs stayed; `Cfg_Load` likewise); `Game_Init` as
  musttail C++; unit 8 `mode_tasks` on `__builtin_setjmp` with a barrier
  (the old "registers came back wrong" was clang tail-jumping after popping
  the frame) and `area_slope`; unit 9 the TEB reads as `NtCurrentTeb()`. 6
  differ, each explained (`stackwatch` two log lines widened, `shell`'s
  `Game_Init`, the two fuzz-only files, the two harnesses' inlined TEB load);
  fourteen module fuzzes at 0 mismatches (`field_e2` 306,000 rounds,
  `scena_calls` 294,000, `battle_misc` 147,500 ...); the whole suite 20 min
  8 s, 0 non-zero mismatch lines; the exe host passed; every B1 file compiles
  for x86-64.

**Defects found by the wave, to number at the PR** (D399 and D400 went to
issues #177 and #178 the same day): `game_clock.cpp:90-99` computes
DIV-0022's expected bytes from `&GetTickCount`, which the 64-bit slot (a thunk
place) will not match - design 4's fix is the slot's current word, before the
first 64-bit run; `__builtin_FUNCTION()` defaults in a shared header change
i686 `.rdata` (the two-signature shape is the rule); `AreaMap_Slope` reads its
caller's argument slots through `__builtin_frame_address` (no x86-64 meaning;
B2); `snapshot.cpp`'s `4 * 0x4000` arena should follow `task_sched`'s
`kStackSize` (stage C sets it from the stackwatch and checks `exe_layout`'s
reserve); `mp3.cpp`'s i686 block lacks `xmm` clobbers (kept byte for byte);
`CreateSoundBuffer`'s out-locals at `sound.cpp:504` / `:904` are zeroed today,
so a 4-byte write fills them on x86-64 - an unzeroed local would keep a stale
high half. Stale text for the docs pass: `THIRD_PARTY.md` 2 ("no audio"),
`wav-dump.md` 1 (the slot check), `pad_sdl.cpp`'s "SDL's only user".

**The merged tree** (`ebc2416e`): builds; `codegen_same.py` against the stage
A tip's build gives the 17 explained objects and no other; the hosted suite,
the hosted suite `'*'` (28,505 lines, 1,095 mismatch lines all 0, no FATAL), the
exe host and the moved base (`0xA00000`, the fence 0x58F000 bytes reserved, 0 held)
re-run on it by the coordinator the same night, each ending "self-test only:
done" (`x641101/round/st2_*.log`). **Wave 2 launched** off it: B2 on `x64/b2`, the brief `x641101\b2\BRIEF.md`.

### Stage B wave 2 as run (2026-11-02 morning, B2 on `x64/b2`, tip `e5ab60e2`, merged; report `x641101/b2/REPORT.md`)

Eight commits, one per item: **437 two-cast narrowings in 270 live files
through `Low32(p)`**, `rest_1c.cpp`'s 28 casts as `Img(X_addr)`, `Low32` on
x86-64 a template over pointer types (function pointers reach it), `Slot32`'s
store and `sound_sdl`'s and `wav_dump`'s local checks folded in (0 differ);
**the one site where `Low32` fires by the design's placement**: `Game_Clock`'s
`expected` bytes from the slot's word instead of `&GetTickCount` (design 4;
the intended differ, `patch ON Game_Clock` in all three runs); `rdata_consts`
reading DirectInput's 32-bit image layouts as word structs of its own,
`gfx_clut`'s `pixels` and `gfx_texcache`'s cells as `Slot32` (the `gfx_clut`
differ diagnosed to the self-test's row init and restored byte for byte by an
aggregate spelling); **`AreaMap_Slope` reading its parameters** instead of
its caller's frame through `__builtin_frame_address` (a `Fatal` for a count
of 16 or more, no caller passes 8; the fuzz draws 0..15 and compares the
return, the scratch and the caller's frame, 131,072 rounds 0 mismatches; no
ledger entry: the behaviour is the same); the generated scalar ctypes
`unsigned long` / `long` as `std::uint32_t` / `std::int32_t` (125 + 18; the
count rows kept, since spelling them fails 23 objects on hand-written helpers
- the remainder); the exit trace's registers through `CtxHex` by width, no
`#if` in the game file; the 229 function-pointer widenings read and left (all
game-table words, none a Windows address); the census re-run (`--check`
clean, 5 narrow rows left, each explained); `using ::bof3::Low32;` at global
scope on x86-64 (the first compile failed 311 objects on the unqualified
name). Gates: `codegen_same.py` 650 objects, 4 differ (`game_clock`,
`area_slope`, `area_slope_fuzz`, `win_main`, all intended); the hosted suite,
the exe host and the moved base passed; **the `x86_64` compile: 0 of 380
objects fail** (4 before). No game file carries a width `#if` outside B1's
asm pairs. Left for the audit: the census wants a `low32` class (it no longer
counts the narrowings), the `long` remainder (23 files), the global `using`
shape, the exit trace's `%s` width.

**Stage B's tree, as gated (2026-11-02 morning, `c7a0a4e7`: wave 2 merged, then
`main` at PR #182 merged in, the SDL3 entry renumbered DIV-0201 because `main`
had taken DIV-0197..0200 for issues #177..#179 the same day; `ledger_check`,
`ledger_registry --check`, `ledger_switches --check` and `width_census --check`
clean):** the i686 build; the hosted suite `'*'` (28,510 lines, every mismatch
line 0, no FATAL), the exe host and the moved base (`0xA00000`), each ending
"self-test only: done" (`x641101/round/st3_*.log`). The PR is at the owner's
word; stage C runs on `x64/c1` off the tip before the merge.

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
3. the FMV (D407, 2026-11-07: this run saw the clip run, not a picture; see
   the assumptions list): `capcom.avi` plays through MCI on the 64-bit host (the Cinepak
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

### Stage C as run, the agent's part (2026-11-01 night, C1 on `x64/c1`, tip `7d6736d0` with `x64/round` `b07085e8` merged; report `x641101/c1/REPORT.md`)

**The 64-bit build links and runs.** The link found 254 undefined symbols,
all from the excluded fuzz files (251 `SelfTest`s behind `WantsShadow`, two
helpers, and `BattlePhases_Inject`, the one inject that lived in a fuzz
file): `WantsShadow` is a constexpr `false` on x86-64 (`detour.h`) and the
inject moved to its live file. The DLL is a PE32+ at `0x10000000` with no
`DYNAMIC_BASE`, `SizeOfImage` `0x590B000`; the exe a PE32+ at `0x400000`
with no `.reloc` and `DllCharacteristics` 0, as XA4 wrote it. The 64-bit
launcher failed with `0xC000007B` until `launcher.manifest`'s
`processorArchitecture="x86"` became `"*"` (and ninja does not rebuild the
`.rc.res` on a manifest edit).

**None of the design's `Fatal`s fired** (`Low32`, `LowBlock`, the bind
table's reach, `ImageBase()`, `rsp`): every address lay below 4 GB by
placement, as design 2 measured. What stopped the runs was a class no census
counted: **32-bit image words read or written through native pointer types**
- `*reinterpret_cast<T**>(place)`, `memcpy(&ptr, place, sizeof ptr)`,
`Get<T*>` / `Put<T*>` accessors, function-pointer arrays over code-word
tables, local structs with `void*` members overlaid on image records
(`Row`, `ClutRow`, `ChapterHooks`), a byte-offset vtable read, `Import<Fn>`
over the import slots, the sound shim's 8-byte out-pointer writes,
`SetVolumeBits` calling a float function through a 32-bit type, and the
SDK's `DIDATAFORMAT` in `rdata_consts`. About 90 sites in 45 files now read
the word on both widths; `slot32.h` gained `CodeSlot<Fn>` for code-word
tables; `Dd_CreatePlainSurface` / `Dd_CreateTextureSurface` take a
`Slot32<void>*` (their `symbols.toml` parameters changed). The texture
cache's `Row` overlay was the attract's thrash: every CLUT generation
mismatched, 64 page snapshots a frame, the frame arena's `Fatal`.

**Self-tests pass on the 64-bit exe host** (exit 0, "self-test only: done",
0 mismatches), on the merged tree too. **The attract's state hash is
identical on all 9,665 compared ticks** between the two hosts of the same
commit (`attract_run.py --no-front --minutes 6 --host exe`, the skip list
applied at record time); the pace 10,789 logic frames in 360 s on x86-64
against 9,205 on i686, back to back. Both hosts differ from the `r14`
reference on the same two pages at the same ticks (`0x5FD000` every tick,
`0x803000` ticks 6,833..7,074), so that distance is the build's, not the
width's. **The stackwatch** on a quit recipe through the attract: the
64-bit slices' deepest writes `0x5518` and `0x54F8` of `0x8000`
(`kSlice64` holds with a third of head room) against `0x1284` / `0x12E8` /
`0x1300` of `0x4000` on i686 - a ratio of 4.6, not the estimated 2.18, so
the deepest route may want `0x10000` (the reserve holds it); the routes'
`BOF3X_STACKWATCH=1` run decides, no constant changed. The i686 run used
three slices where the 64-bit used two, unexplained.

Gates on the merged tree: `codegen_same.py` against a `b07085e8` baseline,
650 objects, 8 differ, each named with its reason and fuzz; the hosted module
fuzzes of every touched module at 0 mismatches; the whole hosted suite
`'*'`; the 32-bit exe host and the moved base; the `x86_64` compile and link
at exit 0. **Defects, described:** XA4's fuzz exclusion against the
inject-in-a-fuzz-file (fixed); the uncounted pointer-overlay class (the
census wants it, stage D); the x86 manifest and its build dependency;
`attract_watch`'s `holdrand @ None` on both hosts (a tool note); the
slice-use difference; `kSlice64` possibly small. **The coordinator's runs**
(items 1..5 above) follow in the next subsection.

### Stage C as run, the coordinator's part (2026-11-01 night and 2026-11-02 morning; root `x641101/proof`)

The builds: C1's `L64` and `L32` (`x64/c1` `7d6736d0`), then `L64f` with the
`Music_Pump` fix below (`x64/c1` `814b01c1`). Every run `--host exe` on the
round's game copy, `--no-front`, hands off; hashed runs under the `orig4`
conditions (`BOF3X_OPT=none BOF3X_LAYERING=0`, the skip list).
`run_routes.sh`, `run_proof2.sh`, `run_quick.sh`, `run_stack.sh`,
`run_fix.sh`, `run_stackq.sh` there, each with its `.out`.

**1. The ten routes, 64-bit against 32-bit (`statehash.py diff`).** Identical
on every tick: combat (2,561), dragonTransform (4,289), menu_screens (1,729),
cutsceneAndNue on its second pair (7,361). The rest differ only on pages that
also differ between two runs of one width:

| Route | x32 / x64 | Read as |
|---|---|---|
| caughFish | `0x6C3000` from tick 395 | five `Gfx_ClutRows` block pointers (+0xB44..+0xB64, heap addresses): raw dumps at 395, 400, 2,000; x64 against x64 and x32 against x32 differ the same way |
| masterAndManillo, worldMapAndAreaTransition | `0x6C3000` from 324 / 1,098 | the same class |
| field_menu | `0x7DE000`, 15 ticks | the sound page (`Music_Finished`'s noise row) |
| cutsceneAndNue (first pair) | VRAM shadow (9 pages), `0x937000`, `0x7DE000`, `0x7C9000` / `0x7CA000` | x64 against its own repeat differs on the same 12 pages; the second x32/x64 pair is identical |
| whelpBoss | the same VRAM / `0x937000` class, on the **32-bit** side (x64 matches the references there) | run noise, either width |
| shop | 11 game pages from tick 2,714 to the end | **a defect**, below; after the fix one tick of `0x7DE000` |

Both widths against the `orig4` pairs report the same signature pages
(the pairs predate the bind table, state-hash.md 8), page for page, on
combat and caughFish.

**The defect: the 64-bit music stopped after its first quarter second.**
`Music_Pump` handed `Music_Events` - two 32-bit image slots - to
`MsgWaitForMultipleObjects`, which reads 8-byte handles on x86-64: one merged
handle and the word after the pair, a failed wait every frame, the ring never
refilled. The music log (`BOF3X_MUSIC_LOG=1`) at f2495 of shop: heard sample
11,520 (18,432 written) on x64 against 3,200,256 on i686; the inn's jingle
never ended, so the morning transition never came (`Transition_Busy` 1 on
i686 at tick 2,714, 0 on x64), the same on three 64-bit runs. Fixed on
`x64/round` (`0bdc1f3e`): on x86-64 the two slots are copied into a native
array first. The i686 spelling is kept by width: the copy passes a stack
address where Capcom's passes the image's, and the sound fuzz's stub records
that address (tried 2026-11-02: 5 `Music_Pump` mismatches in
`BOF3X_SHADOW='*'`), so it is one width `#if` in a game file for stage D's
list. i686 `sound.cpp.obj` 0 differ, the hosted suite `'*'` exit 0. With the
fix, shop's heard sample at f2495 is i686's exactly. **No hash and no attract
saw this:** the ring's words are in the skip list and the routes but one
never wait on a stream's end - stage D's audit wants a check that hears the
music (a census row: an image slot array handed to an API as a native
array; this was the only handle array).

**2. The quicksave** (`quick_combat`, `quicksave_check.py`): on the 64-bit
host the in-process proof is `RESULT: equal` (300 ticks after F7 equal the
300 after F6 but the two expected noise pages) and the cross-process proof
exits 0. A take of the other width is refused both ways: `F7 BISLPS10.DAT
refused: another build - made by v0.1.0 (bof3x.dll 1f676aed5476...), this is
v0.1.0 (0be42825073e...); the save half is still a save`, the overlay
"Quickload refused: another build".

**3. The FMVs.** `CAPCOM.AVI` and `LOGOS.AVI` open through MCI and play to
their ends on the 64-bit host (`DIV-0035 ... into the window at 0,0 640 x
480`, no skip line, `WinMain: FMVs done`) in every run; the round's game
copy holds those two AVIs only. What the window shows is the owner's eye.
**Seen 2026-11-07: no picture for `CAPCOM.AVI`** - the pump ran to the end
over frames MCI could not decode, since Windows has no 64-bit `iccvid.dll`
(D407, [`known-defects.md`](known-defects.md)). The lines above proved the
pump, not the picture.

**4. The launcher.** The 64-bit launcher starts the 64-bit exe beside it in
every run above. The 32-bit launcher with the 64-bit exe and DLL beside it
(`Lmix`) plays the attract, 3,136 logic frames in 120 s. The Sources page
runs `tools/importer.py` as a child and never names the executable, so it
has nothing width-specific; opening it in the 64-bit launcher is the
owner's look. The moved base (`BOF3X_IMAGEBASE`, the developer launcher's
fence) was not run on the 64-bit host.

**5. The stackwatch on the routes** (`BOF3X_STACKWATCH=1`, `L64f`, each route
ending in F9 twice, F9 twice at the title and a `quit`, since the exit lines
need a normal exit): the deepest write in task slice 0 is `0x5508` to
`0x5628` of `0x8000` (cutsceneAndNue the deepest), slice 1 `0x54F8` on every
route, slices 2 and 3 never committed. **`kSlice64` `0x8000` holds**, a
third of it spare at the deepest; no change. field_menu and menu_screens end
at their own last step before the appended quit, so they were not measured.


## 6. Stage D: audit and docs

A read-only audit of every width-conditional line in `src/` (the list is
XA1's census re-run on the round's tip: every `#if` by width in a game file
is either an i686/x86_64 asm pair that [`x64-asm-policy.md`](x64-asm-policy.md)
allows or a logic fork the audit names and explains - on `main` `39d88ef9`,
2026-11-07, there are 16 in 10 files under `src/game`, and the one logic fork
known is `sound.cpp`'s `Music_Pump`, HANDOFF item 14a), of every `Low32()` that fires in no run but
could, and of the ledger: a 64-bit build that behaves as the 32-bit one
needs no DIV entry; anything stage C changed for the player (a refused
quicksave, a `Fatal` on low memory, a sound the shim changed) gets one.
`STATUS.md`, `HANDOFF.md`, [`SCAFFOLDING.md`](SCAFFOLDING.md) 4 (the
presets), `release.yml` (the second artefact, signed as the first), this
doc's "as run" sections.

### Stage D as run (2026-11-07, branch `x64/stage-d` off `main` `39d88ef9`; root `C:\Users\kerok\bof3x-runs\x641107`)

The coordinator's session, with two Opus agents for the contained parts
(the CI and release workflows on `x64/d-ci`, the music-hearing check on
`x64/d-music`, both merged into the branch). Every run on fresh builds of
the branch tip, both widths (`L32`, `L64` under the root; `Lmix` is the
32-bit launcher pair beside the 64-bit exe, DLL and moved exe), on a copy
of the round's game folder, `--no-front`, hands off.

**1. The audit, as a command.** `tools/width_census.py` has four classes
for it since this day (its docstring says what each row is): `width_if`,
`low32`, `handle_array` and `ptr_overlay`; `--list <class>` prints the rows,
`analysis/width_census.tsv` holds them, and `--check` in CI refuses a new
one in a live file. What they say on the tip:

- **Every width `#if` in a game file is an asm pair the policy allows, but
  one.** 16 blocks in 10 files under `src/game`: fifteen `asm_pair` rows -
  the two call tables (`Scenario_CallA`, `Scenario_CallB`; policy 8 unit 3),
  the six ecx entries (`Music_IsPlaying`, `LoadMenu_Error`,
  `ScenaCall_SaveAndLeave`, `Scena15_LeaveAllParty7`, `Music_Halt`,
  `SndStream_IsPlaying`; unit 4), the three callee-table jumps
  (`Sound_StopMusic`, `Sound_ResumeAll`, `Sound_MusicPlaying`; unit 5), the
  two eax thunks (`Battle_StatusTint`, `ScriptFlags_Clear40`; unit 6),
  `Cfg_Load`'s frame (unit 7) and `task_switch.h`'s two includes (unit 2,
  the named exception) - and one `logic` row, `Music_Pump` (section 5's
  defect: the x86-64 half copies the two slots into a native handle array;
  the i686 half keeps Capcom's call so the sound fuzz's stand-in sees the
  image's array). The 30 other blocks are in `src/hook`, `src/audio` and
  `src/common` (the width's `CONTEXT` fields, the detour's `jmp`, the crash
  report's registers, `ImageBase()`, the decoder's stack switch), logic by
  nature and outside the rule.
- **No `Low32()` narrows an address the design places above 4 GB.** 466
  live calls: 175 are the one-line helpers (`Key`, `Addr`, `AddressOf`...),
  whose 1,460 callers the census now lists as `narrow_call` rows; the rest
  by class - image_local 101, object_ours 81, func_ours 35, image 28, pool
  18, windows 12, unread 15, crt 1 (`--counts`). The rows outside the classes
  the low-memory design keeps below 4 GB by placement (design doc 2) were
  read one by one: the TEB's `StackBase` / `StackLimit` (the harnesses; the
  main thread's stack, placed low), `VirtualQuery` results walking below
  16 MB or the main stack (`draw_pool.cpp`, `stackwatch.cpp`, `exe_host.cpp`,
  `snapshot.cpp`), our DLL's own base and tables (`statehash.cpp`,
  `crash.cpp`'s `ImageRange` for the DLL and the main module only,
  `rdata_consts.cpp`'s DirectInput table), the hosted-only tracer and
  `Crt_GetPtd` (32-bit for ever), the game's heap blocks (`mp3.cpp`,
  `display_env.cpp`: `Crt_malloc`, whose `LowBlock` is the chokepoint), the
  render shim's and the SDL layer's objects (`HeapAlloc` of the process heap,
  a static pool), the WAV dump's own hook, and `Slot32`'s store (every slot,
  the trap by design). **Where a Fatal could still fire, in a run no route
  has made:** a process-heap block (the render shim's surfaces and pixels,
  the game's `Crt_malloc`) once the low 4 GB has filled - the probe's first
  block above it came after 2.75 GB of allocation (design doc 2) - and our
  DLL if the loader ever relocated it, which `bind.cpp` refuses at load.
  Nothing else narrows a native address.
- **The two rows the defect asked for.** `handle_array`: three sites in
  the tree - `fmv_play.cpp`'s wait with no array, `Music_Pump`'s two (the
  native copy on x86-64, the image slots on the i686 branch alone,
  `slot_i686`). `ptr_overlay`: 38 live rows, 10 in game files, none over
  the image's bytes - the render shim's `Surface` over a slot's `.Native()`
  (our object, whose address the slot holds narrowed), and the self-tests'
  `FakeObject` / `FakeBuf` stand-ins of ours; the rest are launcher dialog
  states and render objects. C1's 23 `pointer-field` rows stay as they were
  rewritten (a 32-bit word read, then `At()`).
- **The ledger.** Nothing stage C or D changed is player-visible on the
  32-bit default, and the 64-bit build adds no behaviour of its own: a
  quicksave of the other width is refused by the build id, the quicksave
  round's rule for any other build (DIV-0145's take and load, "another
  build" in the overlay); `BOF3X_ORIGINAL` and `BOF3X_SHADOW` are refused with the exe
  host's existing message; the sound output is DIV-0201's on both widths.
  **No DIV entry** for this round beyond DIV-0201, and no defect number.

**2. The runs not made in stage C.**

- **The moved base on the 64-bit host.** The 64-bit player's launcher
  forwards `BOF3X_IMAGEBASE` to `bof3x-launcher-dev.exe`, which that build
  does not make (CMakeLists.txt, `BOF3X_HOSTED_BUILD`), so it stops with the
  "developer launcher's" message. The run is the mixed copy: the 32-bit
  developer launcher beside the 64-bit `bof3x.exe`, `bof3x.dll` and
  `bof3x-A00000.exe` (`build-x86_64` writes the moved one too). Its fence
  reads the child's `SizeOfImage` at the same header offset in a PE32+ as in
  a PE32, and reserved `0x58F000` bytes of `0x400000..0x98F000` with nothing
  already held; the engine's own fence agreed. The exe host's self-test at
  `0xA00000` passed (`host: ... the image at 0xA00000, moved from 0x400000`),
  and `combat` hashed at `0xA00000` against `combat` at `0x400000` recorded
  with `BOF3X_STATEHASH_FOLD=0xA00000` is **identical on all 2,561 ticks**
  (`proof\diff_combat_moved.txt`). Trap: without the fold on the `0x400000`
  side the diff refuses the pair, and `statehash.py check` against the
  `orig4` pairs refuses a moved run for the same reason.
- **The stackwatch on `field_menu` and `menu_screens`** (`L64`): slice 0's
  deepest write `0x5508` of `0x8000` on both, slice 1 `0x54F8`, slices 2
  and 3 never committed - the same figures as the other eight routes, and
  below `cutsceneAndNue`'s `0x5628`; `kSlice64` holds on all ten. Trap paid:
  the stage C copies in `proof\stackq` ended with the recipe's own `end`
  line before the appended F9 / quit lines, so the quit never ran and no
  exit line was written; the `end` removed, both quit and log.
- **The WAV pair under both sound switches** (`L32`, `shop.txt`, `--wav`,
  `tools/wav_diff.py`): the dump under DIV-0201 (SDL3) against the dump
  under `BOF3X_OFF=SoundSdl` (DirectSound) is identical for the first ten
  seconds and differs in 96 of 106 one-second windows from `10.0 s`, where
  the town's song starts (`Music_Start` at f320); the levels match within a
  few dB throughout. A cross-correlation of the two dumps (3 s windows at
  15, 40 and 70 s) finds the same lag in each: **the song lies 546 ms
  (24,084 samples) later in the SDL3 dump than in the DirectSound one**,
  with the effects, which are frame-timed, where they were; after that
  alignment the residual is 7..8 dB under the level. The music logs agree
  but for the inn's resume sample (`3,200,256` under SDL3, `3,197,952` under
  DirectSound: one 2,304-sample fold apart, DirectSound's cursor against
  the computed one). Not settled: whether the ear hears the song that much
  later against a cue (the inn's night, DIV-0169, is a scene with one), or
  whether the dump's model of the computed cursor places the ring's halves
  later than the device plays them. **The owner's listen, 2026-11-07, on the
  64-bit build:** nothing unusual at the inn's night, the combat loop and the voices sound
  right - the figure is the dump's model, not the ear's. It is in
  `proof\wav_diff_shop.tsv` and the two dumps under `wav\`. The one thing the
  owner heard, a quick-load during a battle voice bringing the voice back
  before the song, is the rebuild's order, not the output layer's: D406.

**3. The CI and the release** (`x64/d-ci`): the `x86_64` job of
`build.yml` is a gate like the `i686` one - configure, build, the same
import check (`Arch: x86_64`, system DLLs only; `bof3x.exe` imports
`bof3x.dll` and `KERNEL32.dll` alone), the three binaries uploaded as
`bof3x-x86_64-<sha>`. `release.yml` builds both widths in its one job,
signs all six binaries in the one jsign call, verifies the 64-bit exe's
layout with `make_exe.py --width 64`, and attaches a second zip,
`bof3x-<tag>-win-x64.zip`, to the draft and to `latest` beside the 32-bit
one, whose name and place do not change; `make_release.py` reads the exe's
width and says so in the zip's README. A dry run of the 64-bit zip from
this day's build listed 1,203 files, no game data. The jsign call with six
files is unexercised until the next push to `main`
([`releases/README.md`](releases/README.md)).

**4. The ear beside the hash** (`x64/d-music`): `Sound_Tick` writes a
`music_log ... ring track <t> heard <sample> written <samples> playing|halted`
line once in every 60 frames under `BOF3X_MUSIC_LOG=1`, held during a
self-test-only run so the hosted suite's logs stay as they were, and
`tools/music_heard.py <log>` fails (exit 1) on the stage C defect's shape, a
track playing with no advance of the heard sample, and with exit 2 when the
log has no ring line at all. On the merged tip, rebuilt in both widths, the
ten routes on the 64-bit exe host with the switch pass the check one by one
(`proof\heard_<route>_x64.txt`: `whelpBoss` 164 ring lines over 6 tracks,
`cutsceneAndNue` 116 over 5, `caughFish` 62 over 6, `combat` 40 over 4,
`dragonTransform` 67 over 4, `masterAndManillo` 89 over 6, `field_menu` 20
over 2, `menu_screens` 25 over 2, `shop` 46 over 2,
`worldMapAndAreaTransition` 33 over 5), `shop` on the 32-bit exe host
passes alike (46 over 2), and stage C's `shop_x64m.log` from before the
`Music_Pump` fix exits 2, as the log of a build without the ring line must
(`run_d3.out`). The i686 gate on the same tip: the hosted suite with
`BOF3X_SHADOW='*'` exits 0 plain and under the music log, the exe host's
self-test exits 0 in both widths.

Trap paid (the coordinator's, not the agent's): the hosted `'*'` suite
takes about forty minutes and its log grows by hundreds of lines a minute
the whole way; a run judged hung from one tail and a stale mtime was
killed mid-way and had to be run again. Count the log's lines twice a
minute apart before calling a `'*'` run hung.

## 7. For the owner

**Answered by the owner on 2026-11-01**, after a read of the narrow and wide
roads' upsides and downsides (the chat of that morning, summarised in IDEAS
I53):

1. **(N), the narrow image.** The wide image is ledgered as a future effort,
   IDEAS I53: once the narrow build runs with the proof in hand, a later
   round may widen the records one struct at a time, each step gated by
   the state hash against the narrow build.
2. **x86-64 Windows alone, with B1's stack switch written with a third
   variant in mind**: the `i686` and `x86_64` switches side by side in a
   shape an ARM64 (or any later) variant slots into without a redesign.
3. **A developer's build and a second release artefact** beside the 32-bit
   one; the 32-bit executable stays the player's default until a release
   of play on the 64-bit one.
4. **SDL3.** B5 moves the sound output to SDL3 audio rather than writing a
   DirectSound shim.

The questions as staged, kept for the record:


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
  C's run 3. **It does not** (D407, the owner's first play, 2026-11-07: no
  picture; `iccvid.dll` is in `SysWOW64` alone, and MCI pumps the clip to its
  end drawing nothing). Fixed by DIV-0205: the game's own decoders
  ([`replacing-mci.md`](replacing-mci.md) 8), on both widths.
- The pace cost of `Img()` as a native pointer with a run-time base on
  x86-64: stage C's run 5.
- Whether anything in `src/game` depends on `__stdcall` / `__cdecl` beyond
  the spelling (the aliases of section 0): XA1 lists, XA3 compiles.
- Which naked functions exist only for a Capcom caller's register contract:
  XA3.
- The 2026-10-29 figures that do not reproduce with a recorded command (the
  asm split 125 / 29 / 20, "40 naked in 16 live files", "39 pointer fields",
  "19 narrowings of a function"): replaced by section 0's commands; XA1's
  tool is the one count from then on (done 2026-11-01: section 0 cites
  `width_census.py --counts`, and the 2026-10-31 `grep` figures it corrects
  are kept beside it).
