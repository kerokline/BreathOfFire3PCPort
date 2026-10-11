# State 3: our own executable - the design brief

**Status:** STABLE as a design (2026-10-11, written on `platform/state3` from `main`
`a5946f64` before any code, the owner's five calls answered the same day - section 10; every number below is from a read of this
morning or from the doc it cites - nothing here has been run)

[`HANDOFF.md`](HANDOFF.md) item 0 asks for one document, answering from the
reads already made, before the first build: what the executable is, the
entry-stub page, what the start-up reproduces, the stack, the code page,
testing across the cutover, and what the first build does and does not do.
This is that document. It is the design the build agent works to; the
decisions it leaves to the owner are in section 10. It cites rather than
restates: [`platform-layers-plan.md`](platform-layers-plan.md) section 3
(the three states), [`exe-import-engine.md`](exe-import-engine.md) section 4
(the data map, the stub page, `BOF3X_EXEIMAGE`),
[`exe-resources.md`](exe-resources.md) (what else the file holds),
[`crt-startup.md`](crt-startup.md) sections 9 and 11 (the start-up's
decisions, the stack), [`code-page-audit.md`](code-page-audit.md) section 4,
[`state-hash.md`](state-hash.md) section 7 and
[`state2-proof.md`](state2-proof.md).

## 0. The shape in one paragraph

One engine, two hosts. `bof3x.dll` stays the engine and keeps running inside
the player's `BOF3.exe` as today - that host is where Capcom's bytes exist,
so it stays the home of the shadow fuzz, the A/B switch and the reference
pairs. The new host is **`bof3x.exe`**: a 32-bit image of ours whose own
sections occupy `0x400000..0x93F000`, so Capcom's address range is reserved
by the loader before any heap or thread block can land in it. At start-up
the engine copies the player's `.rdata` and `.data` into that range from the
player's own file (or the importer's cache), writes `jmp ours` at every one
of the 10,088 function starts it owns into a page that was `int3` wall to
wall, binds the thirteen import slots our code still calls through, sets the
x87 word Capcom's runtime set, and calls our `Game_WinMain` on the process's
main thread as Capcom's entry point did. No code of Capcom's is mapped,
loaded or copied to disk. Everything of ours that reads or writes a byte of
Capcom's *code* - the operand patches, the call re-aims, the clones - is a
hosted-only mechanism and is refused loudly in the new host until it is
re-expressed as a value of our own.

## 1. What the executable is

### 1.1 Why the image must occupy the range itself

The plan's words were "our executable reserves the range and loads `.rdata`
and `.data` there" ([`platform-layers-plan.md`](platform-layers-plan.md) 3).
A run-time `VirtualAlloc(MEM_RESERVE)` at our entry point is too late: in a
32-bit process the process heap, the PEB and the first thread's TEB are
placed bottom-up by `ntdll` before any instruction of the image runs, and
with the image elsewhere the lowest free 64 KB blocks are exactly
`0x400000` upward. Today the game is safe because `BOF3.exe`'s own image
holds the range from process creation; DIV-0062's draw pool needed the
launcher to reserve from the *parent* while the child was suspended "with
only its image and ntdll mapped" (`src/launcher/launcher.cpp`,
`ReserveDrawPoolIn`) for the same reason. The one way the range is ours on
every start, double-clicked or launched, is for **our image to be based at
`0x400000` and to span it**: the loader maps it before anything else is
placed. It also makes `GetModuleHandle(nullptr)` equal to `0x400000`, the
`hinstance` Capcom's `WinMain` received and `DInput_Init` hands to
`DirectInputCreateA` (`src/game/pad_read.cpp`), which wants the executable's
module.

### 1.2 The sections

| Section | VA | Size | Protection | Content at load | Who fills it |
|---|---|---|---|---|---|
| headers | `0x400000` | `0x1000` | R | ours | the PE writer |
| `.stub` | `0x401000` | `0x1C3000` | RWX while written, then RX | `0xCC` (`int3`) throughout | the engine: a `jmp` at each owned start (section 2) |
| `.game` | `0x5C4000` | `0x37B000` | RW | zero | the engine: `.rdata` `0x5C4000..0x5DA000` and `.data` `0x5DA000..0x676000` copied in; `0x676000..0x93D6EC` is the `.bss` the original loader zeroed, already zero |
| `.pool` | `0x93F000` | `0x48000` | RW | zero | DIV-0062's 2,048-item draw pool (`draw_pool_room.h`: `0x90` bytes an item), stamped by the entry so `DrawPool_Reserve` finds it where it looks today; below 16 MB as the 24-bit links need |
| `.text` / `.idata` / `.rsrc` | `0x987000..` | small | as usual | the entry, the import table (`KERNEL32`, `bof3x.dll`), the manifest and our icon | the PE writer |

`.game`'s span is the two data sections' virtual extent exactly: `.rdata`
`0x5C4000` + `0x15D22`, `.data` `0x5DA000` + `0x3636EC` to `0x93D6EC`
([`exe-resources.md`](exe-resources.md) 2), rounded to the page. Capcom's
`.rsrc` at `0x93E000` is not reproduced: its one icon group was read only by
`Game_WinMain`'s `LoadIconA`, ours since 2026-09-23, which will load our own
icon from our own `.rsrc` ([`exe-resources.md`](exe-resources.md) 3).

The IAT's 109 dwords at `0x5C4000..0x5C41B4` arrive with the copied `.rdata`
as the file's bytes - lookup-table RVAs, not addresses - and stay dead bytes
except the thirteen slots our code reads (section 3.2). The import
descriptors at `0x5D9378..0x5D9D21` are dead bytes nothing of `src/` names
([`exe-resources.md`](exe-resources.md) 4).

### 1.3 How the image is made

**Recommended: a PE writer of ours in Python, `tools/make_exe.py`**, run as a
CMake custom command after `bof3x.dll`. The file holds no game data - the
section table above is layout, like `symbols.toml`'s addresses - and it is
about 250 lines over `struct`: the headers, four sections, an import
directory naming `KERNEL32.dll` (`ExitProcess`, `GetModuleHandleW`,
`GetCommandLineA`, `GetStartupInfoA`) and `bof3x.dll` (`Engine_Main`), a
`.rsrc` with one `RT_GROUP_ICON` + `RT_ICON` and one `RT_MANIFEST`, and an
entry of a dozen instructions. The build then **verifies its own output**
with the same parser (`tools/pe_data.py` / `pe_funcs.py` already read PEs):
section VAs and sizes as in 1.2, `ImageBase` `0x400000`, no `.reloc`,
`DllCharacteristics` without `DYNAMIC_BASE`, stack reserve and commit as in
section 5 - a mismatch fails the build. The alternative, an assembly file
linked by `lld` with `--image-base 0x400000`, depends on `lld` placing a
custom first section before `.text`, which llvm-mingw's CRT objects (linked
first) and `lld`'s fixed section ranking make doubtful; a Python writer
removes the doubt and the project already has the parsers. The agent may
choose the assembly route if a ten-minute probe shows `lld` honours the
order, provided the same build-time verification runs.

**The engine stays `bof3x.dll`**, imported statically by the executable (the
loader finds it beside the exe: the application directory is first in the
search order). No remote thread, no injection - the loader's own import.
Nothing of the engine's 10,088 implementations changes; what changes is the
host-dependent scaffolding in `src/hook/` (section 7). Linking the engine
*into* the executable as objects was considered and set aside for the first
build: it would need every `GetModuleHandle` / `LogOpen` / crash-dump path in
`src/hook/` rewritten at once, and it would lose the hosted build, which
section 6 needs. A later state can fold the two into one file; nothing here
prevents it.

**The entry** (in the PE writer's `.text`): `push esp` is not needed; it
calls `bof3x.dll!Engine_Main(hinstance = 0x400000, cmdline = GetCommandLineA
past the program name as Capcom's `_wincmdln` produced it, show =
STARTUPINFO's `wShowWindow` if `STARTF_USESHOWWINDOW` else `SW_SHOWDEFAULT`)
and passes its return to `ExitProcess` - the exit code reproduced
([`crt-startup.md`](crt-startup.md) 9, the `exit` row). The engine does the
rest (section 4).

### 1.4 Where the data comes from

The first build reads the two sections **from the player's `BOF3.exe` file**,
verified by SHA-256 as the launcher verifies it today (`kExpectedSha256` in
`launcher.cpp`; the same routine moves into the engine), by the section
table, into `.game`. The file is opened read-only and never copied to disk:
the bytes exist in our process's memory as they do today under the hosted
build (rule 1 holds as it holds now). `BOF3X_EXEIMAGE=<cache>` keeps its
meaning and gains one: in the exe host it is the **source** of `.data`
(`base/exe/data.bin`, `pc-zh` only, as `src/hook/exe_image.cpp` already
checks) rather than a check against a running image; `.rdata` still comes
from the file until the importer writes it too (the cache holds `data.bin`
and `data.toml` only - `ls analysis/cache/pc-plus-us/base/exe`, this
morning). A disc-built image is refused as today (the code pointers and the
22,331 bytes no disc carries, [`exe-import-engine.md`](exe-import-engine.md)
4.3). The data-only cache `BOF3X_CACHE` (DIV-0089) is untouched by this work.

## 2. The entry-stub page

[`exe-import-engine.md`](exe-import-engine.md) 4.2 chose (b), stubs at
Capcom's addresses, over rewriting the 9,142 `.data` code pointers; the
measurements that make it sound are 6,211 of 6,346 targets being function
starts, the smallest gap between starts 11 bytes, and the seven placed
mid-function words being numbers
([`exe-code-pointers-midfunc.md`](exe-code-pointers-midfunc.md)). What the
page is in this design:

- **The fill is `0xCC`.** Every byte of `.stub` the engine does not write is
  `int3`. A code pointer into the middle of a function, a target the
  catalogue missed, a `.rdata` code pointer (127 words, 81 targets, none a
  start - not mapped as code, so if one is ever called it traps), a call
  into the decoder's eight or the runtime's two (section 2's last bullet):
  each lands on a breakpoint that the crash reporter (`src/hook/crash.cpp`,
  a vectored handler) turns into a `CRASH` line naming the address and the
  caller's return address, and the process ends. That is rule 4's "aborts
  loudly" for code that no longer exists.
- **The writes are `Inject`'s.** `WriteJmp` (`src/hook/detour.cpp`) writes
  `E9 rel32` at `original` toward `ours`, exactly as today; `InjectAll` runs
  unchanged and its 10,088 `BOF3_INJECT` lines are the stub table. Two
  checks relax in the exe host only: `CloneOriginal`'s and `PatchBytes`'s
  reading of Capcom's bytes (section 7), and the direction - `BOF3X_ORIGINAL`
  has no meaning without Capcom's code and is a `Fatal` in the exe host, not
  silently ignored.
- **Then read-execute.** After `InjectAll` the page goes `PAGE_EXECUTE_READ`;
  a later write faults. `RetargetCall` and `PatchBytes` into the page are
  refused before that (section 7), so nothing of ours writes there after
  start-up.
- **Two addresses that are not function starts need a stub each**, both
  found this morning:
  1. **`0x5ACBBC`, `DirectSoundCreate`'s import thunk** (`jmp [0x5C4014]`),
     which our `Snd_Init` calls as a function (`sound_rest_callees.h`
     `kDirectSoundCreate`; it was one of the 37 entries of
     [`state2-proof.md`](state2-proof.md) that ran, at frame 0). In the exe
     host it becomes `jmp [slot]` with the slot bound to `DSOUND.dll`
     ordinal 1 (section 3.2), or `sound_rest.cpp` calls the import
     directly. The brief prefers the stub: the fuzz's recorder table keeps
     its address.
  2. **`Imp_GetTickCount` `0x5C407C`** is a `.rdata` slot, not a code
     address - listed here because `game_clock.cpp` `PatchBytes`es it
     expecting `kernel32!GetTickCount` to be bound there (DIV-0022); the
     slot binding of section 3.2 must run before `GameClock_Inject`.
- **The twelve `[[func]]` starts without `impl`** (`exe-import-engine.md`
  4.1's 2026-10-10 list, less `MoveCmd_Move`, taken: the decoder's eight,
  `Crt_ftol`, `Crt_GetPtd`, `Fmv_EnterFullscreen`, `Gfx_PackRgb`) get no
  stub: `int3`. The decoder's eight are reached only under
  `BOF3X_MP3=original`, which the exe host refuses at start-up (DIV-0091's
  switch is an agent's, [`HANDOFF.md`](HANDOFF.md)); `Crt_GetPtd` only when
  `Rand` is left original (`crt_rest.cpp` `RandSeedCell`), impossible here;
  `Crt_ftol` has no caller ([`crt-rest.md`](crt-rest.md)); the last two
  never ran in the state 2 proof. `tools/owned_sweep.py` stays the check that
  nothing new names them.

## 3. What the engine maps and binds before the game starts

### 3.1 `.rdata` and `.data`

Both copied whole, as 1.4 says. `.rdata` is copied although our code reads
its 44 float, double and GUID constants from the engine's own table
(`src/game/rdata_consts.h`, `rdata::Verify()` compares them with the exe's
bytes and keeps doing so - against the copy now): the copy is what makes
`DInput_KeyboardFormat` `0x5C4948` (`c_dfDIKeyboard`, whose object table and
GUIDs are more `.rdata` with absolute pointers into it) and the four GUIDs
read by address valid without further work, and it costs nothing. Item 4 of
[`exe-import-engine.md`](exe-import-engine.md) 5 (the SDK's `c_dfDIKeyboard`
instead) stays open and is not this build's.

`.bss` is zero because `.game` is a zero-filled section; the engine asserts
it (`exe_image.cpp`'s check, kept) rather than trusting it.

### 3.2 The thirteen import slots, and the thunk

`src/` calls through these IAT words today because the loader bound them
([`exe-import-engine.md`](exe-import-engine.md) 2's table): `0x5C4020` /
`24` / `28` (GDI32 `SetTextColor`, `SetBkMode`, `TextOutA` -
`psx_rest_callees.h`), `0x5C407C` (`GetTickCount`, `game_clock.cpp`,
`win_main.cpp` `Tick()`), `0x5C4080` / `84` / `88` (`CreateEventA`,
`CloseHandle`, `GetDriveTypeA` - `sound_callees.h`, `shell_callees.h`),
`0x5C40B8` / `0x5C4114` / `0x5C4158` / `0x5C4174` (`ExitProcess`,
`TerminateProcess`, `PostQuitMessage`, `MessageBoxA` - `win_main.cpp`'s
exit trace and `psx_rest_callees.h`), `0x5C4164` / `68`
(`MsgWaitForMultipleObjects`; the slot `gfx_texcache.cpp` tests for
"bound yet"), plus `0x5C4014` for the thunk of section 2. In the exe host
the engine **binds them itself** before `InjectAll`: a table of
(slot, DLL, name or ordinal), `GetProcAddress` over `GetModuleHandle` /
`LoadLibrary`, the word written into the copied `.rdata`, and a `Fatal` if
any is missing. Fourteen rows; one self-test reads every row back and calls
`GetTickCount` through its slot. The alternative - every call site changed
to call the import directly - touches eight files for no gain in the first
build and loses `gfx_texcache.cpp`'s check; later, with the hosted build
retired, the sites can go that way.

### 3.3 What is not mapped, with the reason

`.text`: no code of Capcom's, by definition. `.rsrc`: read by nobody but our
`WinMain`. The headers: nothing of `src/` reads `BOF3.exe`'s headers but the
hook layer's section walks, which section 7 replaces with constants per
host. The import descriptors, the Rich record, the DOS stub: dead.

## 4. The start-up, and what it reproduces of Capcom's

[`crt-startup.md`](crt-startup.md) 9 is the table; this is the order the
engine's `Engine_Main` runs it in, with `DllMain` reduced in the exe host to
`LogOpen` and the host detection (today `DllMain` does everything, under the
loader lock; in the new host the exe's entry calls `Engine_Main` after the
loader is done, which is also where `CreateThread` for the crash reporter
belongs).

1. **Host detection.** `GetModuleHandle(nullptr)`'s headers: Capcom's
   timestamp `0x3ADD0C06` and `SizeOfImage` `0x53F000` mean the hosted
   build (`VerifyImage` as today); our own identity (a `.stub` section at
   `0x401000`, our own timestamp constant from the PE writer) means the exe
   host; anything else is a `Fatal`. One `bof3::Host()` answer, read by
   section 7's sites.
2. **The layout check:** the four sections at the VAs of 1.2, `.stub` and
   `.game` readable and writable - a `Fatal` otherwise (a rebased or
   rewritten image).
3. **The data:** section 3.1's copy, from the file or the cache; the
   SHA-256 of the file before any byte of it is used; the `.bss` zero
   assertion.
4. **The slots:** section 3.2.
5. **The x87 control word `0x027F`** - precision 53 bits, every exception
   masked, round to nearest - set with `fldcw` (or `_controlfp` with
   `_MCW_PC | _MCW_EM | _MCW_RC`) and **read back and asserted**; our
   toolchain's start-up leaves `0x037F` (`_fpreset` as `fninit`,
   [`crt-startup.md`](crt-startup.md) 4.1, measured). MXCSR is logged
   (`0x1F80` under Capcom's start-up, [`crt-startup.md`](crt-startup.md)
   11), not set. The stack watch's start line already prints both.
6. **The draw pool:** the entry stamped `.pool`; `DrawPool_Reserve` looks
   there in the exe host (one candidate) instead of
   `draw_pool_room::kCandidates`.
7. **The state hash's census** before the injects (`StateHash_BeforeInjects`,
   reading `.data` as the "loader" - now the engine - mapped it), then
   `InjectAll`, then the census after, as today.
8. **The page goes read-execute.**
9. **The exception handling that is ours**, the row "SEH frame" of the
   table: the vectored crash reporter is already armed; the executable's
   entry has no SEH frame, so an exception nobody handles reaches Windows'
   default - a `SetUnhandledExceptionFilter` that logs and ends with code 3
   (`Fatal`'s) replaces Capcom's `__except` that `_exit`ed silently, and the
   runtime's R6xxx message boxes are gone with the runtime (the table's
   "replace with ours (`Fatal`)" row).
10. **`Game_WinMain(0x400000, nullptr, cmdline, show)`** on the main thread -
    the thread `Task_SetStackBase` measures from (section 5). Its return is
    the exit code.

Dropped, per the table, with nothing to do: the Windows version words, the
heap, the per-thread data (`holdrand` is ours; `errno` unread), the locks,
`_cfltcvt_init`, `_adjust_fdiv`, the I/O handle table, `stdio`,
`_acmdln` / `__argv` / `_pgmptr` (never read by the game - the entry still
hands `WinMain` the command line, as 1.3 says), the environment, the
multibyte table, `_cinit` / `_initterm` (no C++ constructors in the game),
the `atexit` table, the signal filter. **Reproduced: no `setlocale`**
([`code-page-audit.md`](code-page-audit.md) 1: nothing of ours calls one;
the exe's start-up may not either - the import check of section 6.4
covers it).

## 5. The stack

The PE header's values are the writer's to set: **reserve `0x100000`,
commit `0x10000`** - [`crt-startup.md`](crt-startup.md) 11.3's floor
(reserve >= `0x40000`, commit >= `0x10000`) with the toolchain's usual
reserve, against the measurement of 11.4 (the F9 quit route: deepest dword
`0xD578` below base, the initial `0x10000` commit never grown, slice 3
untouched; the four task slices need `0x14294` below base, so a `0x40000`
reserve holds them and the commit holds slices 0..2). The game runs on the
process's main thread exactly as under Capcom's entry; no fiber, no second
thread (the render fiber `RunOnFiber` keeps its own 1 MB stack as today).
`BOF3X_STACKWATCH=1` keeps working with one change: it finds the main thread
today by the suspended context holding `0x5BA057` in `eax`
(`src/hook/stackwatch.cpp`); in the exe host the main thread is the current
one, and the entry of interest is `Engine_Main`.

## 6. Testing across the cutover

### 6.1 The headless self-tests

`BOF3X_SELFTEST_ONLY=1` on the executable runs steps 1..8 of section 4 and
every start-up self-test that does **not** clone Capcom's code, then ends the
process (exit 0, or 3 on a `Fatal`). `BOF3X_SHADOW` set in the exe host is a
`Fatal` naming the hosted build - a clone needs Capcom's bytes
(`CloneOriginal` copies them from the address) and there are none; a silent
skip would make an exe-host "exit 0" mean less than a hosted one. **The
fuzz stays on the hosted build:** `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*'
build/bof3x-launcher.exe --game <dir> --no-config` as today
([`SCAFFOLDING.md`](SCAFFOLDING.md) 2), and the build is green only when
both hosts pass. New self-tests the exe host adds: the layout check, the
data copy's byte count against the file's section sizes, the fourteen slot
bindings read back, the x87 word read back, the `.bss` zero assertion, and
a `PatchBytes` / `RetargetCall` inventory of zero (section 7).

### 6.2 The state hash

The check is address-independent since `BOF3SH3`
([`state-hash.md`](state-hash.md) 7): a pointer of ours in one of Capcom's
26,189 slots hashes by the text it names, and the stub page keeps every
code pointer's *value* what it is today, so the two hosts hash alike by
construction. The references, in order:

1. **The same engine build in both hosts**, the attract sequence
   (`attract_cycle.txt`, 7 minutes) and the ten routes, Chinese,
   `BOF3X_OPT=none`, `--no-front`: identical expected on every page but the
   noise pages the doc names (the sound page `0x7DE000` one tick at a time,
   `Sound_Channels`' heap pointers). This is the cutover's own A/B and the
   first thing run.
2. **Capcom's pairs:** `attract_r14_orig` / `_origb` on the attract sequence,
   `cn_<route>_orig2` / `_orig2b` on the ten routes (`analysis/statehash`;
   the pairs were re-recorded 2026-10-10 against the owner's saves,
   [`HANDOFF.md`](HANDOFF.md) item 2). `BOF3.exe` is still runnable as its
   own process for a fresh pair whenever one is needed - the anchor
   survives the cutover, as the plan said.
3. **Under a language overlay**, `en_<route>_sh3a` / `_sh3b` (2026-10-11
   morning) with `BOF3X_LANG=en-US`: the exe host against a reference of
   ours, which is what an overlay's text pointers allow.

From the first build on, the day-to-day check is build N against build N + 1
in the exe host, with a hosted run beside it while the hosted build is kept.

### 6.3 The tooling that names the process

`tools/attract_run.py` finds and kills the game as `BOF3` with
`bof3x.dll` among its modules (`kill_stale`, `game_pid`), and
`tools/recipe_saves.py` lists `IMAGENAME eq BOF3.exe`; `input_run.py`,
`close_probe.py`, `ini_probe.py`, `key_probe.py` go through the launcher.
Each needs to know both names (`BOF3.exe`, `bof3x.exe`) - a small change
with the same rule as today: kill by pid, never by image name
([`SCAFFOLDING.md`](SCAFFOLDING.md) 2). `input_run.py`'s `--launcher`
stays the way to pick a build directory; the launcher decides the host
(section 8).

### 6.4 CI

`.github/workflows/build.yml` fails if a binary imports anything but system
DLLs; the executable imports `bof3x.dll` and must be allowed that one. The
build's own section-table verification (1.3) runs there too, so a wrong
layout never reaches the owner's machine. Nothing in CI runs the game.

## 7. What of ours assumes Capcom's bytes, measured this morning

A session script over `src/game` mapped every `PatchBytes` and
`RetargetCall` whose target lies in `0x401000..0x5C4000` to the `[[func]]`
that contains it. **Every one lies inside a function that has an `impl`**
(ours): the patch changes Capcom's copy for the A/B switch and the clone,
while our implementation carries the same divergence - and in one case
reads the patched operand back to learn which way the switch is set
(`yes_no_layout.cpp:368` reads the five bytes at `0x5747F0` and `Fatal`s if
they are neither the original's nor DIV-0027's). `PatchBytes` refuses unless
the `expected` bytes are present, so in the exe host every such site would
`Fatal` on `0xCC`. The inventory, so the build agent knows its size:

| Mechanism | Sites into `.text` | Where |
|---|--:|---|
| `PatchBytes`, literal address | 19 | `cheats.cpp` 2 (`Steal_Start`, `SkillSteal_Roll`), `config_text.cpp` 10 (`Config_DrawRowLabel`, `Config_DrawRowOptions`, `Config_DrawControllerRow`, `Config_DrawControllerPanel`), `draw_pool.cpp` 1 (`AreaMapBD_BuildView`), `gfx_filter.cpp` 2 (`Display_Setup`), `yes_no_layout.cpp` 4 (`Menu_YesNo`, `Menu_DrawSaveSlot`) |
| `PatchBytes`, table-driven | 13 + 3 + 18 + the config labels | `draw_pool.cpp` `kSites` (13, DIV-0062's pool addresses), `title_menu.cpp` `kInsnAt` (3), `widescreen.cpp` `kSites` (4) and `kSlides` (14), `config_text.cpp` `kLabelSites` |
| `RetargetCall` | 14 | `battle_draw.cpp` 2, `config_text.cpp` 3, `menu_frame.cpp` 4, `menu_verbs.cpp` 1, `text_advance.cpp` 1, `yes_no_layout.cpp` 3 |
| Direct read of code bytes | 1 | `yes_no_layout.cpp:368` |
| `CloneOriginal` | 203 | every fuzz file, under `BOF3X_SHADOW` only |
| `.text` section walks of the main module | 6 files | `crash.cpp`, `calltrace.cpp`, `detour.cpp`, `exe_image.cpp`, `stackwatch.cpp`, `statehash.cpp` (constants per host instead) |

The other 23 `PatchBytes` sites write our own DLL variables (`&g_...`), and
`pause_text.cpp`'s four and `widescreen.cpp`'s first table write `.data`
and `.rdata` words - mapped, so they keep working.

**The rule for the exe host:** a divergence's state is a value of ours, not
a byte of Capcom's. Each site above becomes one of:

- **dead in the exe host** - the patch only served Capcom's copy (the
  `RetargetCall`s re-aim calls inside functions that are ours, and
  `cheats.cpp`'s immediates patch a function whose `impl` already takes the
  cheat from `BOF3X_STEAL`): `PatchBytes` / `RetargetCall` return without
  writing when `Host()` is the exe and the target is in `.stub`, and
  **log** it, so the inventory is visible in every log; or
- **a variable** - where our implementation read the operand back
  (`yes_no_layout.cpp:368`, `widescreen.cpp`'s operand copies, `title_menu.cpp`'s
  `g_now`), the implementation reads its own switch instead, set by the same
  `Inject` function, and the hosted build keeps the byte patch for Capcom's
  copy beside it.

Both are mechanical and the ledger entries stay as they are: no behaviour
changes, only where the switch lives. **The self-test asserts the inventory
is zero** once the agent has been through the list: a `PatchBytes` or
`RetargetCall` reaching `.stub` in the exe host after that is a `Fatal`, so
a future takeover cannot reintroduce the pattern unnoticed.

The decoder's `kOriginals` table in `sound.cpp` holds Capcom's `Mp3_*`
addresses by name through `symbols.gen.h`; they are never called in the exe
host (section 2) and the values point into `int3`. `game_clock.cpp` and
`gfx_texcache.cpp` read import slots (3.2 binds them first).
`sound_rest_callees.h`'s `kDirectSoundCreate` gets its stub.

## 8. The launcher's role after the cutover

Today: verify `BOF3.exe`, read the default key table from it, the ini and
dialog, write `BOF3.CFG`, set the environment, create `BOF3.exe` suspended,
reserve the pool, inject, resume. **In the first build, one line changes:**
it creates `bof3x.exe` (beside itself) with the game directory as the current
directory and no suspension, no reservation, no remote thread - the
executable does the rest. Everything else stays: the dialog is the settings
UI ([`launcher-settings.md`](launcher-settings.md)), the environment is how
settings reach the engine, `BOF3.CFG` is still the game's own input. A
`--host bof3` switch keeps the hosted start for the fuzz and the A/B pairs
while the hosted build exists. The player-facing shape the handoff names - a
config dialog over an exe of ours, the exe runnable alone with the ini the
launcher wrote - follows once the engine reads `bof3x.ini` itself; not the
first build's.

## 9. The code page

[`code-page-audit.md`](code-page-audit.md) 4: the cache folder is ANSI by
design today; at state 3 the executable's manifest declares
`<activeCodePage>UTF-8</activeCodePage>` and `longPathAware`, the engine logs
`GetACP()` once (an older Windows that ignores the manifest is then visible
in `bof3x.log` and behaves as today), and the launcher's half comes with it:
its `cache=` stored as UTF-8 (`MultiByteToWideChar(CP_UTF8)` in `FromAnsi` /
`ConfigCacheDataRoot`, `SetEnvironmentVariableW`), `ToAnsi`'s refusal
removed, a once-only rule for an existing ini's ANSI bytes (invalid UTF-8
read as the ACP), and the length limits (`kRootMax`, the dialog's 226)
counted in UTF-8 bytes or at the `W` boundary in UTF-16 units. **The first
build ships both halves** (the owner, 2026-10-11). The PE writer emits the
manifest anyway for the common-controls and DPI declarations the launcher
already carries (`src/launcher/launcher.manifest`). The check: a cache folder
whose name the ACP cannot hold, set in the dialog, opened by the engine under
`BOF3X_CACHE` - a path the hosted build refuses today.

## 10. For the owner - answered 2026-10-11

The five calls, with the owner's answers of the same day:

1. **The executable's file name and window title.** Recommended `bof3x.exe`
   beside the launcher, the window title unchanged (DIV-0039's). **The
   owner: the name is fine** - read as `bof3x.exe`; see the note below.
2. **The icon.** Ours, not Capcom's (rule 1). **The owner will supply the
   artwork**; the first build ships a plain placeholder until it arrives.
3. **UTF-8** (section 9): **now, both halves** - the manifest line in the
   executable and the launcher's ini and environment half in the same
   build.
4. **The data source order** (1.4): the player's `BOF3.exe` file first, the
   cache's `data.bin` when `BOF3X_EXEIMAGE` names one. **Agreed.**
5. **A ledger entry**, tier Sensible, "the game runs as our own executable",
   listing what the platform under the game no longer does. **Agreed**; the
   build writes it as the next free `DIV-` number at its merge.

*Note on the name:* the owner wrote `bof3.exe`. The player's own file is
`BOF3.exe`, and Windows file and process names are case-insensitive: the two
would be told apart only by directory, and every tool that lists the game by
image name (`tasklist IMAGENAME eq BOF3.exe` in `recipe_saves.py`,
`Get-Process BOF3` in `attract_run.py`) would see both as one. The brief
keeps `bof3x.exe` unless the owner confirms `bof3.exe` knowing that.

## 11. What the first build does and does not do

**Does:** `tools/make_exe.py` and its CMake step with the build-time
verification; `Engine_Main` and the host detection; the data copy from the
player's file and from `BOF3X_EXEIMAGE`; the fourteen slot bindings and the
thunk's stub; the `0xCC` fill, `InjectAll` as the stub writer, the page
read-execute; the x87 word set and asserted; the `.pool` section and
`DrawPool_Reserve`'s exe-host candidate; the exception filter; section 7's
inventory worked through with the self-test assertion; the launcher's one
line and `--host`; the UTF-8 manifest and the launcher's ini half (section 9);
the placeholder icon; the ledger entry; the tooling's second process name; CI's allowance. Then
the checks in this order: both hosts' headless self-tests exit 0; the
attract sequence in the exe host against the hosted build of the same engine
and against `attract_r14_orig` / `_origb`; the ten routes against
`cn_<route>_orig2` / `_orig2b` and, under `en-US`, against `en_<route>_sh3a`
/ `_sh3b`; a `BOF3X_STACKWATCH=1` run of the F9 quit route in the exe host
for the depth line. The title reached with the two FMVs, the display and
the sound is what the attract run shows. **Then the owner plays it.**

**Does not:** fold the engine into the executable; read `.rdata` or the
code pointers from a disc build; make `BOF3X_CACHE` the install's data
source; change any call site from an import slot to a direct import; take
`c_dfDIKeyboard` from the SDK; retire the hosted
build - it stays the fuzz harness until a later state replaces the clone
fuzz with something that needs no Capcom bytes; 64 bits or another system
(`PLAN.md` phase 4, I33).

## 12. Not verified

Everything: this is a design from reads. In particular, the claim that the
loader places the heap and thread blocks bottom-up into `0x400000..` when the
image is elsewhere (1.1) is the standard behaviour of 32-bit Windows and the
reason DIV-0062 reserves from the parent, but it was not measured today - a
ten-line probe (an image based at `0x10000000` printing `VirtualQuery` over
`0x400000..0x93F000` at its entry) would settle it and belongs in the build's
first hour, before the PE writer; a result of "always free" would still not
change the design, only its justification. Whether `lld` can place a custom
first section (1.3) is likewise a probe, not a fact. The `int3` fill's
interaction with the call tracer's own `int3`s (`BOF3X_CALLTRACE` plants one
at each armed entry and recognises its own by address) is a read the agent
makes before arming the tracer in the exe host.
