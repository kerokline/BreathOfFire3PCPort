# The C runtime's start-up and exit, read for the cutover

**Status:** STABLE (2026-10-10) - a read pass, on `crt/atoi-and-startup`
from `432bada8`. No code; nothing of Microsoft's runtime decompiled. Every
address is `BOF3.exe`'s, read with capstone (`tools/pe_disasm.py`, a
scratch annotator naming the import slots from the image's import directory,
`tools/pe_xref.py` over `analysis/pc_xref.json` for who touches a global,
and an E8 / E9 scan of `.text` for who calls a start). **Measured
2026-10-10 night** on `prework/stack-measure`: the main thread's stack
(section 11) and the start-up control words (section 4.1), with the
instrument `BOF3X_STACKWATCH` for the depth a live run uses.

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
     will not give it unasked: its start-up leaves `0x037F` (measured,
     section 4.1);
  2. **a main-thread stack** deep enough for the four 16 KB task stacks the
     game carves below `WinMain`'s frame (section 8). Windows gives the game
     0x40000 bytes, not the header's 0x10000 (measured, section 11);
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
does not give it (section 4.1: `0x037F` at `main` unless `CRT_fp8.o` is
linked; the cutover should set the word itself and assert it). The
`_cfltcvt` hooks and `_adjust_fdiv`: drop.

### 4.1 The words measured (2026-10-10)

| Where | x87 control word | MXCSR | How |
|---|---|---|---|
| `BOF3.exe`'s main thread, suspended at its first instruction | **`0x027F`** | `0x1F80` | its context (`GetThreadContext`, `FloatSave.ControlWord`, MXCSR at `ExtendedRegisters + 24`), logged by the DLL at start-up: `stack: main thread's first context` |
| The DLL's thread at `InjectAll` (the launcher's `CreateRemoteThread`) | `0x027F` | `0x1FA0`; `0x1FA7` under `BOF3X_SHADOW='*'` | `fnstcw` / `stmxcsr` there: the `fpu:` line. The low bits are sticky exception flags (`0x20` precision), raised by something the DLL ran on that thread before (not traced); the masks are the default |
| A host program of our toolchain at `main` (llvm-mingw, clang 22.1.8, UCRT) | **`0x037F`** | `0x1F80` | `fnstcw` / `stmxcsr` in `main`; the same linked `-static` |
| The same, with the toolchain's `CRT_fp8.o` linked | `0x027F` | `0x1F80` | `i686-w64-mingw32-clang++ cw.cpp <toolchain>/i686-w64-mingw32/lib/CRT_fp8.o` |

So **Windows starts a thread at `0x027F`** (both threads measured), and `_fpmath`'s
`_controlfp(_PC_53, _MCW_PC)` leaves the word as it found it on this OS -
the word the game ran under is Windows' default, which Capcom's start-up
also asks for. Our toolchain's start-up then **changes** it: mingw-w64's
start-up calls `_fpreset`, and the default `CRT_fp10.o` defines that as
`fninit; ret` (`llvm-objdump -d CRT_fp10.o`: `db e3 c3`), which loads
`0x037F` - 64-bit precision. `CRT_fp8.o` defines `_fpreset` as a jump
through the runtime's import (`__imp___fpreset`, `llvm-nm`) instead and `main` sees `0x027F`. State 3:
link `CRT_fp8.o`, or `fldcw` the word before `WinMain` - and assert
`0x027F` there either way, since a dependency of the link is not a promise.
The DLL's `fpu:` and first-context lines are the reference the first
build compares against.

The probe (scratch, not in the tree):

    // cw.cpp
    #include <cstdio>
    int main() {
        unsigned short cw; unsigned mxcsr;
        asm volatile("fnstcw %0" : "=m"(cw));
        asm volatile("stmxcsr %0" : "=m"(mxcsr));
        std::printf("main: x87 control word 0x%04X, MXCSR 0x%08X\n", cw, mxcsr);
    }

    i686-w64-mingw32-clang++ -O2 -o cw.exe cw.cpp && ./cw.exe
    main: x87 control word 0x037F, MXCSR 0x00001F80

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
  `SizeOfStackCommit` 0x10000; Windows gives **0x40000 reserved, 0x10000
  committed and two guard pages** (measured, section 11). State 3 must give
  its game thread at least that: a reserve of 0x40000 or more and a commit
  of 0x10000 or more (section 11.3).
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

- The main thread's stack: **measured 2026-10-10 night** (section 11):
  0x40000 bytes reserved (`0x000B0000..0x000F0000`), 0x10000 committed,
  two guard pages - Windows' 256 KB floor over the header's 64 KB, which
  cannot hold the arena by itself. **Still owed: the depth a run uses**,
  from the next route run with `BOF3X_STACKWATCH=1` (section 11.4). The
  slices are 16 KB each regardless, and a 13 KB frame of ours on one
  overran into the next task's context on 2026-10-10 evening
  ([`mp3-decoder.md`](mp3-decoder.md) 1) - so any code of ours called from
  a task keeps its frames small or brings its own stack, as the decoder now
  does.
- The toolchain's own start-up control word: **measured** (section 4.1):
  `0x037F`, `0x027F` with `CRT_fp8.o`. The cutover's first build still
  asserts the word on its game thread.
- Whether anything of ours depends on the ANSI code page through the
  toolchain's runtime. **Audited on 2026-10-10:**
  [`code-page-audit.md`](code-page-audit.md) - the "C" locale holds (no
  `setlocale` in `src/` or either binary's imports), no game text meets a
  code page, the install path is reached only relatively; the cache folder
  stays ANSI until state 3 (its section 4); two log / launcher repairs.

## 11. The main thread's stack, measured (2026-10-10)

`src/hook/stackwatch.cpp`, at start-up after `InjectAll`, on the DLL's
thread while the game's main thread is still suspended at its first
instruction. The main thread is the one whose suspended context has the
entry point `0x5BA057` in `eax` (Toolhelp's thread list, `GetThreadContext`;
the log says which rule found it); its TEB comes from
`NtQueryInformationThread(ThreadBasicInformation)`, checked on the DLL's
own thread against `fs:[0x18]`; the words are `NT_TIB`'s `StackBase` +0x04
and `StackLimit` +0x08 and `DeallocationStack` +0xE0C. Headless:

    BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*' <ini-less dir>/bof3x-launcher.exe --game <game> --no-config

    stack: header reserve 0x10000 commit 0x10000; main thread 11108 (eax = entry) base 0x000F0000
      limit 0x000E0000 dealloc 0x000B0000 (reserved 0x40000 bytes, committed 0x10000,
      below it 0x2000 bytes committed guard)
    stack: main thread's first context: eip 0x76EFC000 eax 0x005BA057 (entry 0x005BA057)
      esp 0x000EFFF0 (0x10 below base); x87 control word 0x027F, MXCSR 0x00001F80
    stack: main thread's reservation by region: 0x000B0000+0x2E000 reserve/0x0,
      0x000DE000+0x2000 commit/0x104, 0x000E0000+0x10000 commit/0x4

The same addresses on every run of 2026-10-10 (six).

### 11.1 Why 0x40000: Windows' floor

The header asks 0x10000 / 0x10000; Windows reserved 0x40000. A probe of our
toolchain (scratch: an i686 program printing its own TEB's `StackBase -
DeallocationStack`, linked `-Xlinker --stack -Xlinker <reserve>,<commit>`,
run on this machine, Windows 11 26200, WoW64) gives the rule:

| Header reserve, commit | Reserved |
|---|---|
| 0x8000, 0x1000 | 0x40000 |
| 0x10000, 0x1000 | 0x40000 |
| **0x10000, 0x10000** (`BOF3.exe`'s) | **0x40000** |
| 0x20000, 0x10000 / 0x20000, 0x20000 / 0x30000, 0x30000 | 0x40000 |
| 0x3F000, 0x3F000 | 0x40000 |
| 0x40000, 0x10000 | 0x40000 |
| 0x41000, 0x41000 | 0x100000 |
| 0x50000, 0x1000 / 0x50000, 0x40000 | 0x50000 |
| 0x50000, 0x50000 | 0x100000 |
| 0x61000, 0x1000 | 0x70000 |
| 0x100000, 0x1000 / 0x100000, 0x100000 | 0x100000 |
| 0x110000, 0x110000 | 0x200000 |

That is: the reserve is raised to a **0x40000 floor**, rounded to 64 KB,
and only then, if the commit reaches it, rounded up to 1 MB beyond the
commit. `BOF3.exe`'s commit (0x10000) is below the floor, so it gets the
floor and no more. The commit is honoured as asked (0x10000), and two guard
pages (`PAGE_READWRITE | PAGE_GUARD`, 0x104) sit under it. Where the floor
lives (the loader's, WoW64's or the kernel's) is not measured; the numbers
are this machine's, and a Windows that dropped the floor would hand
`BOF3.exe` 64 KB - see 11.2 for why that would not run.

### 11.2 The arithmetic: what the arena needs

`Task_StackTop` is only set at run time, so the requirement is computed from
the frames between the thread's start and `Task_SetStackBase`:

- **Above the entry**: the thread starts with `esp` 0x10 below `StackBase`
  (`0x000EFFF0`, the first context); Windows' thread-start frames then call
  the entry point - their depth is in the live line (11.4), not here.
- **Capcom's entry `0x5BA057`** (capstone): `push ebp`, the SEH frame's
  three pushes (`-1`, the scope table `0x5D8D38`, `_except_handler3`
  `0x5BC7E8`) and the old `fs:[0]`, `sub esp, 0x58`, three registers:
  **0x78 bytes**; then `WinMain`'s four arguments and the return address:
  **0x14**.
- **`WinMain`**: Capcom's `0x4FCB00` is `sub esp, 0xC4` and four registers
  (0xD4); **ours** (`_Game_WinMain@16` in `bof3x.dll`, `llvm-objdump`) is
  `push ebp; mov ebp, esp`, three registers, `and esp, -16`, `sub esp,
  0x160`: 0x170 plus up to 0xC of alignment.
- **`Game_Init`**: its return address (4); it ends in a tail jump to
  `Task_SetStackBase`, so the arrival `esp` is `Game_Init`'s entry `esp`.

So `Task_SetStackBase` arrives **0x164 bytes below the entry's `esp` under
Capcom's `WinMain`, 0x200..0x20C under ours**, plus the thread-start frames
and their 0x10. `Task_StackTop` is 0x4000 below that, and the slices take
0x4000 each below it:

| Down to | Below `StackBase` (ours, before the thread-start frames) |
|---|---|
| `Task_StackTop` | about 0x4210 |
| slices 0..2 (every `Task_Create` caller passes 0..2, [`task_sched.md`](task_sched.md)) | about 0x10210 |
| all four slices | about 0x14210 |

**The header's 64 KB cannot be the whole story**: even three slices end
past 0x10000, and the fourth lies wholly beyond it. Windows' 0x40000 holds
the four with about 0x2B000 to spare (less the guard pages and whatever the
deepest call below a slice takes). The commit matters too: the initial
0x10000 holds slices 0 and 1 and all but the lowest ~0x210 bytes of slice 2;
slice 2's bottom and slice 3's top fall in the two guard pages
(0x10000..0x12000 below `StackBase`). Windows grows a stack only through its
guard page - a first touch below it faults (why compilers probe frames over
a page) - and every slice's first touch is `Task_Create`'s entry store at
its top, inside the commit or the guard. Today that holds by margin of the
commit, not by design.

### 11.3 For state 3

- **Reserve 0x40000 or more** for the game thread. The toolchain's default
  is 0x100000 reserve, 0x1000 commit (`llvm-objdump -p` of the probe), so
  the reserve is already enough.
- **Commit 0x10000 or more**, or touch the arena top-down before the first
  `Task_Create`: with the toolchain's 0x1000 commit, `Task_Create(1)`'s
  store at about 0x8200 below `StackBase` lies past the guard page unless
  something earlier on the thread already went that deep (today's
  `Fmv_Play` / `Display_Setup` may; nothing promises it). Link with
  `-Xlinker --stack -Xlinker 0x100000,0x10000`, or run the game on a fiber
  or thread created with a stated commit.
- The live depth (11.4) says how much of the reserve a run uses; the
  reserve is fixed then with that number and a margin.

### 11.4 `BOF3X_STACKWATCH=1`: the depth a run uses

Instrumentation (`src/hook/stackwatch.cpp`), off by default; any value but
`1` is a `Fatal`. Armed, it adds:

- **at start-up**, a check of the armed path on the DLL's own thread:
  `Task_SetStackBase` called once with `ecx` / `edx` planted - it must log,
  leave `eax` = the `Task_StackTop` it stored and keep the two - then
  `Task_StackTop` put back as it was (the game sets it itself):
  `stackwatch: armed: ... (the armed path checked on this thread: eax =
  Task_StackTop, ecx and edx kept)`. That line's own `Task_SetStackBase`
  line names the DLL's thread and says `OUTSIDE the main thread's
  reservation`, as it should;
- **at `Task_SetStackBase`** (ours, `task_sched.cpp`: a byte test before
  the original's three instructions; armed, a call after the store with
  `pushfl` / `pushal` around it, so `eax`, the flags and every other
  register leave as the original's): the caller's return address, the
  arrival `esp` and its depth below `StackBase` - **the `WinMain` depth
  11.2 could only bound** - `Task_StackTop`, the slices' bottom against the
  reservation, and the committed low end now. On the main thread's first
  call it copies what the committed part of the slices holds, so the exit
  can tell later writes from earlier ones (WinMain's callees before
  `Game_Init` - the AVIs, the display set-up - leave their frames there);
- **at exit** (`DllMain`'s detach, on the thread that called `ExitProcess`):
  the committed run under `StackBase` again (`VirtualQuery`, a page at a
  time; and the TEB's `StackLimit` when the exit is on the main thread) -
  the guard moves down as pages commit, so **`StackBase` less the committed
  low end is the deepest page any code on the thread touched**; the deepest
  non-zero dword (the same, to the dword: committed stack pages start
  zero); and per slice whether it lies in the committed range and **the
  deepest dword that changed since the copy**, i.e. how much of its 0x4000
  bytes a task (or a `WinMain` callee reaching below `Task_StackTop`) used.

Headless, the switch parses and the start-up lines log (exit 0; `2` gives
`FATAL: BOF3X_STACKWATCH wants 1, got "2"`, exit 3). **The
`Task_SetStackBase` and exit numbers are owed to the next route run**, e.g.
the owner's recorded route with `BOF3X_STACKWATCH=1` added; the exit line
needs a normal quit (a `Fatal` or the crash reporter ends the process
without the detach). Not covered: the render fiber (`RunOnFiber`) has its
own 1 MB stack and is not watched; `BOF3X_ORIGINAL=Task_SetStackBase`
leaves Capcom's and loses the start line.

