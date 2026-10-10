# The C runtime's start-up and exit, read for the cutover

**Status:** STABLE (2026-10-10) - a read pass, on `crt/atoi-and-startup`
from `432bada8`. No code; nothing of Microsoft's runtime decompiled. Every
address is `BOF3.exe`'s, read with capstone (`tools/pe_disasm.py`, a
scratch annotator naming the import slots from the image's import directory,
`tools/pe_xref.py` over `analysis/pc_xref.json` for who touches a global,
and an E8 / E9 scan of `.text` for who calls a start).

State 3 of [`platform-layers-plan.md`](platform-layers-plan.md) section 3 is
our own executable mapping `BOF3.exe`'s data sections and running none of its
code - so none of the start-up between the entry point `0x5BA057` and
`WinMain` `0x4FCB00` runs either. This is what that code does, what it
leaves in `.data` / `.bss`, and for each piece whether state 3 must
**reproduce** it, **replace** it with the toolchain's own, or can **drop**
it. [`crt-rest.md`](crt-rest.md) is the other half: the entries the game
calls.

## 1. Headline

- **Nothing of the game reads the runtime's data.** Of the 249 data
  addresses the runtime's code (`0x5B9380..0x5C4000`) references, the only
  one any other code references is the import slot of `CloseHandle`
  `0x5C4084` (game code `0x5A7109`) - no `errno`, no `_iob`, no `__argv`,
  no `_environ`, no locale word (a join over `analysis/pc_xref.json`, the
  runtime's references against everyone else's). No game or decoder
  function installs an SEH frame (`push 0x5BC7E8`, the runtime's
  `_except_handler3`, appears only in runtime starts). And `src/` names no
  address in the runtime's data runs outside comments (a scan of every hex
  constant in `src/` against the runs of section 9). The game reaches the
  runtime only through the entries [`crt-rest.md`](crt-rest.md) section 1
  lists.
- **What state 3 must reproduce is behaviour, not data:**
  1. **the x87 control word `0x027F`** (53-bit precision, all exceptions
     masked) on the game thread before `WinMain` - section 4. The toolchain
     will not give it unasked;
  2. **a main-thread stack** deep enough for the four 16 KB task stacks the
     game carves below `WinMain`'s frame (section 8);
  3. **`WinMain`'s arguments and exit**: `hInstance` the image base, the
     process ending with `WinMain`'s answer through `ExitProcess`
     (section 6) - ours uses only `hInstance` and answers 1;
  4. **the "C" locale** for the entries bound to the toolchain's
     (`_stricmp`, text-mode `fgets`): never call `setlocale` (section 5).
- **`Rand`'s seed is already ours** (`crt_rest.cpp`, from 1 as `_initptd`
  sets it); the per-thread data, the heap, the locks, the low-level I/O
  table, `stdio`, `argv` / `environ`, the multibyte code page and the
  floating-point conversion hooks are the runtime's own and are dropped
  with it once the entries that use them are ours or the toolchain's - the
  heap only when the MP3 decoder goes ([`crt-rest.md`](crt-rest.md)
  section 2.6).
- **No C++ static constructors and no `atexit` registrations.** The
  `__xc` table is empty, and nothing writes the `_onexit` table's bounds
  (section 6).

## 2. The entry point `0x5BA057..0x5BA153`

`WinMainCRTStartup`, MSVC 6 shape (linker 6.0 in the optional header;
subsystem 2, GUI). In order:

| At | Call / store | What |
|---|---|---|
| `0x5BA05A..0x5BA07A` | `push -1; push 0x5D8D38; push 0x5BC7E8; mov fs:[0], esp` | SEH frame: scope table `0x5D8D38`, handler `_except_handler3` `0x5BC7E8` |
| `0x5BA07D` | `GetVersion` (`[0x5C40B4]`) | writes `0x7DE9EC` (the minor byte), `0x7DE9E8` (major), `0x7DE9E4` (major << 8 + minor), `0x7DE9E0` (the high word) - `_osver` / `_winver` / `_winmajor` / `_winminor`. Each address has one reference, the store: **nothing reads them** |
| `0x5BA0B0` | `0x5BB3DF(1)` `_heap_init` | section 3; 0 -> `0x5BA184(0x1C)` |
| `0x5BA0C2` | `0x5BACFD` `_mtinit` | section 3; 0 -> `0x5BA184(0x10)` |
| `0x5BA0D5` | `mov [ebp - 4], 0` | the `__try` around the rest |
| `0x5BA0D8` | `0x5BEE76` `_ioinit` | section 5 |
| `0x5BA0DD` | `GetCommandLineA` -> `0x93D6E8` | `_acmdln` |
| `0x5BA0E8` | `0x5BED44` -> `0x7DE95C` | the environment block, ANSI - section 5 |
| `0x5BA0F2` | `0x5BEAF7` `_setargv` | section 5 |
| `0x5BA0F7` | `0x5BEA3E` `_setenvp` | section 5 |
| `0x5BA0FC` | `0x5BE74E` `_cinit` | section 4 (the FPU) and section 6 (the initialiser tables) |
| `0x5BA108` | `GetStartupInfoA` into the frame | `nShowCmd`: `wShowWindow` if `dwFlags & 1`, else 10 (`SW_SHOWDEFAULT`) (`0x5BA116..0x5BA124`) |
| `0x5BA10E` | `0x5BE9E6` `_wincmdln` | `lpCmdLine`: `_acmdln` past the program name (quoted or not) and the blanks after it |
| `0x5BA12B` | `GetModuleHandleA(0)` | `hInstance` |
| `0x5BA132` | `call 0x4FCB00` | `WinMain(hInstance, 0, lpCmdLine, nShowCmd)` - ours since round one (`win_main.cpp` `Game_WinMain`, which reads only `hInstance`) |
| `0x5BA13B` | `0x5BE77B` `exit(eax)` | section 6 |
| `0x5BA140` | filter: `0x5BE86E` `_XcptFilter(code, info)` | section 7 |
| `0x5BA154` | handler: `0x5BE78C` `_exit(code)` | section 7 |

Two early-failure exits: `0x5BA184` (`fast_error_exit`: `_FF_MSGBANNER`
`0x5BF032` and `_NMSG_WRITE` `0x5BF06B` when `__error_mode` `0x7DE964` asks,
then `ExitProcess(0xFF)`) and `0x5BA15F` (`_amsg_exit`: the same messages,
then `[0x672940]` - `_aexit_rtn`, initialised to `_exit` `0x5BE78C` - with
`0xFF`). `__app_type` `0x672944` is 2 in the image (GUI) and `__error_mode`
`0x7DE964` is `.bss` 0, never written (its four references are reads), so a
runtime error is a "Runtime Error!" message box (`0x5BF06B`'s
`GetModuleFileNameA` and string building, then the box), not a write to
`stderr`.

## 3. The heap, the per-thread data and the locks

**`_heap_init` `0x5BB3DF`:** `HeapCreate(flags, 0x1000, 0)` - `flags` is
`HEAP_NO_SERIALIZE` (1) only when the argument is 0, and the entry passes 1,
so 0 (`0x5BB3E3..0x5BB3F0`) - into **`_crtheap` `0x93D6E0`**; then
`__heap_select` `0x5BB297` into **`__active_heap` `0x93D6E4`**. The select
answers 1 (`__SYSTEM_HEAP`) at once when `GetVersionExA` reports platform 2
(NT) and major version 5 or more (`0x5BB2C0..0x5BB2D5`); only older systems
read `__MSVCRT_HEAP_SELECT` (`0x5D8E40`) and the image's link-time default.
So on every Windows the game runs on today the small-block heaps
(`0x5BB43C` for 3, `0x5BBF83` for 2) are never set up, and `malloc`
`0x5B9660` -> `_heap_alloc` `0x5B969E` takes the `__active_heap == 1` path
`0x5B976D`: `HeapAlloc(_crtheap, 0, (size + 0xF) & ~0xF)` (0 bytes asks
16). `calloc` `0x5BA221` and `free` `0x5B9577` read the same two words.

**`_mtinit` `0x5BACFD`:** `_mtinitlocks` `0x5BC650`, then `TlsAlloc` into
**`__tlsindex` `0x672950`**, `calloc(1, 0x74)` for the main thread's block,
`TlsSetValue`, `_initptd` `0x5BAD51` on it, and `+0 = GetCurrentThreadId()`,
`+4 = -1`. **The block** (MSVC 6's `_tiddata`, 0x74 bytes, zeroed by
`calloc`) as this binary uses it:

| Offset | Set by | Read by |
|---|---|---|
| `+0` thread id, `+4` handle (-1) | `_mtinit`, `_getptd` | - (no `_beginthread` in the binary: [`crt-rest.md`](crt-rest.md) section 2.1) |
| `+8` `errno`, `+0xC` `_doserrno` | the runtime's failing calls | `_errno` `0x5BCA43` (`_getptd() + 8`), `__doserrno` `0x5BCA4C` (`+ 0xC`): 52 call sites, all in the runtime |
| `+0x14` `holdrand` = 1 | `_initptd` `0x5BAD5C` | Capcom's `Rand` `0x5B93D2`; ours keeps its own seed |
| `+0x50` `_pxcptacttab` = `0x675080` | `_initptd` `0x5BAD55` | `_XcptFilter` `0x5BE87B` |
| `+0x54` `_tpxcptinfoptrs`, `+0x58` `_tfpecode` | `_XcptFilter` around a signal handler | `_XcptFilter` |

`_getptd` `0x5BAD64` makes a thread's block on its first call (the same
`calloc` / `TlsSetValue` / `_initptd`), or ends the process through
`_amsg_exit(0x10)` (`0x5BADB7`); it saves and restores `GetLastError`
around `TlsGetValue`. Its callers are `Rand`, `_errno`, `__doserrno` and
`_XcptFilter` - nothing else (the E8 scan).

**The locks.** `_lock(n)` `0x5BC679` uses `_locktable` at `0x674998 + 4n`:
an entry still 0 is filled with a `malloc(0x18)` critical section under lock
`0x11` (the table's own). `_mtinitlocks` `0x5BC650` initialises the four
entries the image binds statically - `0x67499C` (n = 1), `0x6749BC` (9, the
heap's: `calloc`'s small-block path locks 9 at `0x5BA28D`), `0x6749CC` (13,
the exit lock: `_lockexit` `0x5BE842` pushes `0xD`), `0x6749DC` (17, the
table's) - whose `CRITICAL_SECTION`s are `.bss` `0x7DE970`, `0x7DE9A0`,
`0x7DE9B8`, `0x7DE988`.

**State 3:** drop all of it, at the cutover's terms. The heap and its two
words go when `malloc` / `calloc` / `free` are the toolchain's (gated on
the decoder, [`crt-rest.md`](crt-rest.md) section 2.6); the TLS slot, the
block and the locks go with the last runtime code that reads them (`errno`
is read by nothing of the game's, the seed is ours). Our executable's own
runtime makes its own.

## 4. The floating-point set-up: `_cinit`'s first step

`_cinit` `0x5BE74E` first calls `[0x672930]` if non-zero - `_FPinit`,
linked in because the program uses floating point, here `_fpmath`
`0x5B93F4`:

1. `0x5B940C` (`_cfltcvt_init`): six pointers `0x672954..0x672968` to the
   float conversion helpers (`0x5BB1F4` twice, `0x5BAE9E`, `0x5BAF04`,
   `0x5BAE44`, `0x5BAEEC`). Readers: `_output` `0x5BA4F3` (`%e` / `%f` /
   `%g`) and the input engine `0x5BCF64` (`sscanf`'s `%f`). The game's
   formats have no float ([`crt-rest.md`](crt-rest.md) section 2.2) and
   `sprintf` is ours; `sscanf` goes with the decoder.
2. `0x5BAE1B`: `IsProcessorFeaturePresent(0)` through `GetModuleHandleA
   ("KERNEL32")` / `GetProcAddress` (strings `0x5D8E0C`, `0x5D8DF0`; the
   Pentium FDIV test `0x5BADDD` if the import is missing) into
   **`_adjust_fdiv` `0x7DE958`** - one reference, the store: **nothing reads
   it**.
3. `0x5BADCB`: `_controlfp(0x10000, 0x30000)` (`0x5BF4DA`) - `_PC_53` under
   `_MCW_PC`: **the x87 precision to 53 bits**. Then `fnclex`.

The x87's reset word (`fninit`) is `0x037F` (64-bit precision, all
masked); this step leaves **`0x027F`**, the word measured on the game thread
on 11 million live calls ([`STATUS.md`](STATUS.md), the GTE batch) and the
one every x87 body of ours was fuzzed under. **State 3 must reproduce it**,
explicitly, on the game thread before `WinMain`: the toolchain's start-up
does not promise it (mingw-w64's resets the x87 to its own default, which
is 64-bit precision unless an object like its `CRT_fp8.o` is linked - not
measured here; the cutover should set the word itself and assert it). The
`_cfltcvt` hooks and `_adjust_fdiv`: drop.

## 5. I/O, the command line, the environment, the code page

**`_ioinit` `0x5BEE76`** - the low-level handle table:
`__pioinfo[0]` `0x93C5A0` = `malloc(0x480)`, 32 entries of 0x24 bytes
(`+0` the OS handle, -1; `+4` the flags, 0; `+5` 0x0A; `+8` 0) and
**`_nhandle` `0x93C6A0`** = 32; then inherited handles from
`STARTUPINFO.cbReserved2` / `lpReserved2` (more 0x480 blocks into
`0x93C5A4...`, up to 0x800 handles), then slots 0..2 from `GetStdHandle(-10
/ -11 / -12)`, flags `0x81` plus `0x40` for a character device or no usable handle,
`0x08` for a pipe (`0x5BEFB6..0x5BF01F`), and `SetHandleCount(_nhandle)`.

**`__initstdio` `0x5BCB17`** (an `__xi` initialiser, section 6): **`_nstream`
`0x93D6C0`** = 0x200 (`.bss` 0, so the default), **`__piob` `0x93C6B4`** =
`calloc(0x200, 4)`, its first 20 entries the `_iob` `FILE`s `0x674BD8 +
0x20 * i` (to `0x674E58`), and `_iob[0..2]._file` (`0x674BE8 + 0x20 * i`)
set to -1 where the handle table holds -1 or 0.

[`crt-rest.md`](crt-rest.md) section 2.4: every stream the game opens is
the toolchain's since the file layer moved, and no game code reads either
table (section 1). **State 3: drop both.**

**The environment** - `0x5BED44` (`__crtGetEnvironmentStringsA`): the wide
block (`GetEnvironmentStringsW`, converted with `WideCharToMultiByte` into a
`malloc`ed copy), else the ANSI one copied; which one worked is remembered
in `0x7DEB24` (1 / 2). The copy goes to `_aenvptr` `0x7DE95C`.
`_setenvp` `0x5BEA3E` counts the strings not starting with `=`, builds
**`_environ` `0x7DE9FC`** (a `malloc`ed pointer array and a `malloc`ed copy
of each), frees `_aenvptr` and zeroes it, sets `__env_initialized`
`0x93C6A4` = 1.

**The command line** - `_setargv` `0x5BEAF7`: `GetModuleFileNameA` into
`0x7DEA20` (0x104 bytes), `_pgmptr` `0x7DEA0C` = that buffer; the parse
`0x5BEB90` run twice (count, then fill a `malloc`ed block) over `_acmdln`
(or the module name when it is empty); **`__argv` `0x7DE9F4`**, **`__argc`
`0x7DE9F0`**. Each of `__argc`, `__argv`, `_pgmptr` has one reference, the
store: **nothing reads them**. `_wincmdln` `0x5BE9E6` gives `WinMain` its
`lpCmdLine`; ours ignores it.

**The code page** - the parse, `_setenvp` and `_wincmdln` first call
`__initmbctable` `0x5C17A6` if `0x93C6A8` is 0 (it is also the second `__xi`
initialiser): `_setmbcp(-3)` `0x5C13CE`, the **system's ANSI code page**
(`GetACP` through `0x5C157B`, `__fSystemSet` `0x7DEBF4` = 1), `GetCPInfo`,
into `__mbcodepage` `0x93C35C`, `__ismbcodepage` `0x93C36C`, `__mblcid`
`0x93C584`, `_mbctype` `0x93C480` (257 bytes; lead-byte flags at
`0x93C481 + c`) and `_mbcasemap` `0x93C380`. So on a Chinese system the
runtime's multibyte tables are GBK's (936) and on an English one 1252's.
Readers of `_mbctype` (a byte search of `.text` for the displacement): the
command-line parse (`0x5BEBD1`, `0x5BEC16`, `0x5BECF9`, `0x5BED12`),
`_ismbblead` `0x5C139D`, and `0x5C3503` (called from `0x5C2F8E`, the
environment search) - **none on a game path**. The game draws its text with
its own GBK font; the runtime's locale stays "C" (`__lc_handle`
`0x7DEC10..` all 0: [`crt-rest.md`](crt-rest.md) section 2.3), `__mb_cur_max`
`0x675064` stays 1 (section 2.7 there), whatever the code page.

**State 3:** drop the handle table, `stdio`, `environ`, `argv` and the
multibyte tables; ours reads its settings with `GetEnvironmentVariableA` and
needs no `argv`. **Keep the "C" locale** for the entries now bound to the
toolchain's (`_stricmp` folds `A..Z` only under it; text-mode `fgets`):
our executable must not call `setlocale`.

## 6. Static initialisers, `exit` and the exit path

**`_cinit` `0x5BE74E`**, after `_FPinit`: `_initterm(0x5DA008, 0x5DA014)`
(`__xi_a..__xi_z`, the C initialisers) then `_initterm(0x5DA000, 0x5DA004)`
(`__xc_a..__xc_z`, the C++ ones). `_initterm` `0x5BE854` calls every
non-zero word in `[begin, end)`. In the image:

| Table | Words | Calls |
|---|---|---|
| `__xi` `0x5DA008..0x5DA014` | `0`, `0x5BCB17`, `0x5C17A6` | `__initstdio`, `__initmbctable` |
| `__xc` `0x5DA000..0x5DA004` | `0` | **nothing: no C++ static constructors** |
| `__xp` `0x5DA018..0x5DA020` | `0`, `0x5BCBBF` | `_endstdio` (pre-terminators) |
| `__xt` `0x5DA024..0x5DA028` | `0` | nothing |

**`exit(code)` `0x5BE77B`** is `doexit(code, 0, 0)` `0x5BE79D`:
`_lockexit`; if `_C_Exit_Done` `0x7DEA1C` is already 1,
`TerminateProcess(GetCurrentProcess(), code)`; else `_C_Termination_Done`
`0x7DEA18` = 1, `_exitflag` `0x7DEA14` = 0; then, not quick: the `_onexit`
table from `__onexitend` `0x93C6AC` down to `__onexitbegin` `0x93C6B0`,
calling each - **but nothing in the executable writes either word** (one
and two references, all reads, in `doexit`), so **no `atexit` function is
ever registered** and the walk is skipped (`0x5BE7DC`); the `__xp` table:
`_endstdio` `0x5BCBBF` - `_flushall` `0x5BCE0C`, and `_fcloseall`
`0x5C09A1` only if `_exitflag` is set, which `exit` leaves 0; the `__xt`
table (empty); `_C_Exit_Done` = 1 and `ExitProcess(code)` (`0x5BE83A`).
`_exit(code)` `0x5BE78C` is `doexit(code, 1, 0)`: the `__xt` table and
`ExitProcess`, no flush.

**The game's exit:** ours (`win_main.cpp`) ends `WinMain` with
`Display_Teardown`, `Sound_Shutdown`, `DInput_Shutdown` and `return 1`, as
Capcom's did (`0x4FD000`); the runtime then flushes its streams (none open:
ours are the toolchain's) and calls `ExitProcess(1)`. **State 3:
reproduce** the visible part - the process exits with `WinMain`'s answer;
the flush and the tables have nothing to do. The early-failure messages of
section 2 are replaced by our own `Fatal` / crash reporter.

## 7. Exceptions and signals

The entry's `__except` (filter `0x5BA140`, handler `0x5BA154`) wraps
everything from `_ioinit` to `exit`, `WinMain` included. **`_XcptFilter`
`0x5BE86E`** looks the exception code up in the thread's action table
(`_getptd() + 0x50` -> `_XcptActTab` `0x675080`, 12-byte rows `{code,
signal, action}`, `_First_FPE_Indx` `0x6750F8` = 3, `_Num_FPE` `0x6750FC` =
7): access violation -> `SIGSEGV`, illegal instruction and privileged
instruction -> `SIGILL`, the seven x87 codes `0xC000008D..0xC0000093` ->
`SIGFPE`. **Every action in the image is 0, `SIG_DFL`**, and nothing writes
the table by its address (one reference, `_initptd`'s store of the pointer);
with `SIG_DFL` the filter answers `UnhandledExceptionFilter(info)`
(`0x5BE99F`). When that answers "execute handler" (no debugger), the
handler is `_exit(GetExceptionCode())` - the process ends with the
exception code and no dialog, the trap [`HANDOFF.md`](HANDOFF.md) records
(`0x5BA15F` on the stack; `BOF3X_EXITTRACE`). Our crash reporter
([`crash-reporter.md`](crash-reporter.md)) logs from its vectored handler
before this filter runs.

**State 3: replace** with our own top-level handling (the crash reporter
and a deliberate exit); nothing of the game raises or catches a signal.

## 8. What the game itself depends on

From the game's side - Capcom's game code (now all ours) and ours:

- **No runtime data** (section 1). Every dependency is through a call, and
  every call is an entry of [`crt-rest.md`](crt-rest.md) section 1:
  fourteen ours or the toolchain's since 2026-10-06, `Crt_atoi` since 2026-10-10,
  five left (`malloc` / `free`, `_getptd`, `sscanf`, `_ftol`), all with the
  decoder or the map_cells check.
- **The x87 word** (section 4): every float computation of the game and of
  our 44 inlined `_ftol` sequences assumes `0x027F`.
- **The stack.** The task arena lies inside the main thread's stack:
  `Task_SetStackBase` puts `Task_StackTop` at its `esp - 0x4000` and the four
  tasks take 0x4000 bytes each below it (`symbols.toml` `Task_StackTop`,
  `Task_Create`); [`HANDOFF.md`](HANDOFF.md)'s traps (`RunOnFiber` for
  DXGI). The image's header asks `SizeOfStackReserve` 0x10000 and
  `SizeOfStackCommit` 0x10000 (read from the optional header); the stack the
  game actually gets under those numbers is **not measured here** (Windows
  enlarges a reservation that its commit fills; the live `esp` and the
  thread's stack limits in game would settle it). State 3 must give its
  game thread at least what the game uses today - measure, then link with
  that reserve or run the game on a thread or fiber of a stated size.
- **The heap**, through `Crt_malloc` / `Crt_free` by name while the decoder
  runs (section 3). No assumption about addresses: pointers already differ
  run to run.
- **The command line, the environment, the show flag**: not read by the
  game (`Game_WinMain` ignores all but `hInstance`); our settings come by
  `GetEnvironmentVariableA` straight from Windows.
- **The image has no TLS directory, no load-configuration directory and no
  relocations** (the optional header's data directories: import, resource
  and IAT only) - nothing the loader runs on the runtime's behalf.

## 9. The decisions in one table

The runtime's data, by the runs its code references (`tools/pe_xref.py`
joined over the runtime's starts; runs within 0x40 bytes merged):

| Item | Code | Writes | State 3 |
|---|---|---|---|
| SEH frame, `__except` | `0x5BA05A`, `0x5BA140`, `0x5BA154` | stack only | replace: our handler / crash reporter |
| Windows version words | `0x5BA07D..0x5BA0A9` | `0x7DE9E0..0x7DE9EC` | drop (never read) |
| Heap | `_heap_init` `0x5BB3DF`, `__heap_select` `0x5BB297` | `_crtheap` `0x93D6E0`, `__active_heap` `0x93D6E4` | drop when `malloc` / `free` are the toolchain's (after the decoder) |
| Per-thread data | `_mtinit` `0x5BACFD`, `_initptd` `0x5BAD51`, `_getptd` `0x5BAD64` | `__tlsindex` `0x672950`; the 0x74-byte block (heap) | drop (`holdrand` is ours; `errno` unread by the game) |
| Locks | `_mtinitlocks` `0x5BC650`, `_lock` `0x5BC679` | `_locktable` `0x674998..`; `0x7DE970`, `0x7DE988`, `0x7DE9A0`, `0x7DE9B8` | drop |
| FPU precision | `_fpmath` `0x5B93F4` -> `_controlfp` `0x5BF4DA` | the x87 control word | **reproduce: `0x027F` before `WinMain`** |
| Float conversion hooks | `_cfltcvt_init` `0x5B940C` | `0x672954..0x672968` | drop (no float format; `sscanf` with the decoder) |
| `_adjust_fdiv` | `0x5BAE1B` | `0x7DE958` | drop (never read) |
| Low-level I/O table | `_ioinit` `0x5BEE76` | `__pioinfo` `0x93C5A0..`, `_nhandle` `0x93C6A0` | drop (the file layer is the toolchain's) |
| `stdio` | `__initstdio` `0x5BCB17`, `_endstdio` `0x5BCBBF` | `_nstream` `0x93D6C0`, `__piob` `0x93C6B4`, `_iob` `0x674BD8..0x674E58` | drop |
| Command line | `GetCommandLineA` at `0x5BA0DD`, `_setargv` `0x5BEAF7`, `_wincmdln` `0x5BE9E6` | `_acmdln` `0x93D6E8`, `__argc` / `__argv` `0x7DE9F0` / `0x7DE9F4`, `_pgmptr` `0x7DEA0C`, `0x7DEA20` | drop (never read by the game) |
| Environment | `0x5BED44`, `_setenvp` `0x5BEA3E` | `_aenvptr` `0x7DE95C`, `_environ` `0x7DE9FC`, `0x7DEB24`, `0x93C6A4` | drop |
| Multibyte code page | `__initmbctable` `0x5C17A6`, `_setmbcp` `0x5C13CE` | `0x93C35C`, `0x93C36C`, `0x93C380..`, `0x93C480..`, `0x93C584`, `0x93C6A8`, `0x7DEBF4` | drop (no game reader) |
| "C" locale | none: `__lc_handle`, `__mb_cur_max`, `_pctype` never written | - | **reproduce: no `setlocale`** |
| C / C++ initialisers | `_cinit` `0x5BE74E`, `_initterm` `0x5BE854` | through the two `__xi` functions above | drop (no C++ constructors) |
| `exit` / `_exit` | `doexit` `0x5BE79D` | `0x7DEA14`, `0x7DEA18`, `0x7DEA1C` | **reproduce the exit code** (`WinMain`'s answer); the rest has nothing to do |
| `atexit` table | read in `doexit` only | `0x93C6AC` / `0x93C6B0` never written | drop (nothing registered) |
| Runtime error messages | `0x5BA184`, `0x5BA15F`, `0x5BF032`, `0x5BF06B` | - | replace with ours (`Fatal`) |
| Signals | `_XcptFilter` `0x5BE86E` | the block's `+0x54` / `+0x58` | drop (every action `SIG_DFL`) |

One more consumer of all this, for the record: `_findfirst`'s time
conversion `0x5B992F` -> `__loctotime_t` `0x5BCA55` -> `__tzset` `0x5C03DF`
(once, flag `0x7DEBE8`) -> `_tzset` `0x5C040D`, which reads `TZ` from
`_environ` (`getenv` `0x5C2564`, the name at `0x5D9194`), parses it with
`atol` `0x5B9A10` (`0x5C05CE`, `0x5C05FC`, `0x5C0627`) or asks
`GetTimeZoneInformation`. `Crt_findfirst` / `Crt_findnext` are the
toolchain's since 2026-10-06, so under ours this chain does not run.

## 10. Not measured

- The main thread's effective stack size (section 8).
- The toolchain's own start-up control word in our executable (section 4):
  a one-line check in the cutover's first build.
- Whether anything of ours depends on the ANSI code page through the
  toolchain's runtime (our code is narrow-string `A` calls throughout; not
  audited here).
