# The ANSI code page and the C locale in our own code

**Status:** STABLE (2026-10-10) - an audit of `src/` (the DLL and the
launcher) on `prework/code-page-audit` from `a30f1916`, for state 3 of
[`platform-layers-plan.md`](platform-layers-plan.md) section 3. It answers
the last unmeasured item of [`crt-startup.md`](crt-startup.md) section 10:
whether anything of *ours* depends on the process's ANSI code page or on
the C locale through the toolchain's runtime. Two small port repairs made
(section 3); no game behaviour changed, so no ledger entry.

## 1. Headline

- **The C locale holds, and nothing of ours can change it.** No `setlocale`,
  `_wsetlocale`, `_create_locale` or `_setmbcp` in `src/` (grep), and none
  in the import tables of `build/bof3x.dll` or `build/bof3x-launcher.exe`
  (`llvm-readobj --coff-imports`, section 2.2) - so neither SDL3 nor
  minimp3 nor the D3D11 path nor libc++ calls one either: all are linked
  statically and would have to import it from `api-ms-win-crt-locale`.
  Measured in a probe built with the same toolchain (section 2.1): the
  locale is `"C"`, `___lc_codepage_func()` 0, `MB_CUR_MAX` 1, and
  `isalpha` / `isspace` / `isdigit` / `isupper` / `islower` / `tolower` /
  `toupper` classify or fold **none** of `0x80..0xFF`, passed as `int` or
  as a sign-extended `char`.
- **No game text meets a code page.** The GBK text and the overlays'
  Latin-1 / Shift-JIS bytes (DIV-0064, `src/game/labels.cpp` and siblings)
  are drawn by the game's own font engine; no `MultiByteToWideChar` /
  `WideCharToMultiByte` / `mbstowcs` in `src/game/` (grep), and the
  strings our code hands to ANSI UI calls are ASCII - Display_ErrorBox's
  23 reachable error texts and its caption have **0** bytes `>= 0x80`
  (counted in `BOF3.exe`, section 2.3).
- **The install path is safe whatever its characters.** The launcher is
  wide throughout (`-municode`, `CreateProcessW` with the game directory as
  the current directory); the game and the DLL reach the install only by
  **relative** path, and a narrow relative `fopen` / `_findfirst32` /
  `CreateFileA` under a current directory outside the code page
  **succeeds** (measured, section 2.1). Only `GetCurrentDirectoryA` would
  turn it into `?`, and nothing of ours calls it.
- **The cache folder (`BOF3X_CACHE`, DIV-0087 / DIV-0089) is ANSI by
  design** and stays so at state 2: the dialog refuses a path the code
  page cannot hold, the DLL opens it with `A` calls and fails loudly on a
  `?`. Section 4 is the design for lifting it at state 3.
- **Two defects fixed** (section 3): log lines that named a wide path with
  `%ls` were dropped whole for any character above U+00FF; and the
  launcher widened the ini's `cache=` a byte a character, so a cache
  folder named outside Latin-1 had its layers looked for in another folder.

## 2. Findings

Counts: **path / text calls** - 38 Windows `A` call sites and 12 narrow
CRT path sites (grep of `src/`, comments excluded): 13 see the cache
folder (ANSI by design), 1 a module's path (`crash.cpp` 87), 4 relative
paths under the install (safe), the rest ASCII literals or developer
variables;
**case / ctype** 7 sites, none on bytes `>= 0x80` under a locale that can
fold them; **conversions** 2 (both stated `CP_ACP`, one added `CP_UTF8`);
**number parsing / formatting** 6 float sites, all `'.'` under `"C"`.

### 2.1 The probe

`probe.cpp` (scratch, not committed), built with the project's
`i686-w64-mingw32-clang++ -O2 -static`, run on this machine (ACP 1252, OEM
437), never calling `setlocale`:

| Check | Result |
|---|---|
| `setlocale(LC_ALL, nullptr)`, `___lc_codepage_func()`, `MB_CUR_MAX` | `"C"`, 0, 1 |
| ctype and case on `0x80..0xFF`, as `int` and as signed `char` | 0 classified or folded |
| `_stricmp("\xC4", "\xE4")`, `_stricmp("AB", "ab")` | -32 (not folded), 0 |
| `snprintf("[%ls]")` of U+00E9 / U+4E2D / U+20AC | 3 bytes `5B E9 5D` / **-1** / **-1** |
| `strtof("1.5")`, `strtof("1,5")`, `snprintf("%.2f", 0.25)` | 1.5, 1, `0.25` |
| `CompareStringA(tr-TR, NORM_IGNORECASE, "I", "i")`, the same under `LOCALE_INVARIANT` | 2 (equal), 2 |
| A file in `%TEMP%\bof3x-cp-` U+4E2D U+0416 U+20AC: the path through `WideCharToMultiByte(CP_ACP)` | lossy; `fopen`, `CreateFileA`, `GetFileAttributesA` all **fail** |
| The same file by relative name with that folder current | `fopen`, `_findfirst32`, `CreateFileA` all **succeed** |
| `GetCurrentDirectoryA` there | ends `2D 3F 3F 80` (`-??€`) |

The `%ls` row is the defect of section 3.1: the UCRT's "C" locale has no
character above U+00FF, so the conversion fails and `vsnprintf` answers -1.

### 2.2 Import tables

`llvm-readobj --coff-imports` over both binaries, filtered for locale,
case, conversion and path entries:

- **Neither imports `setlocale`, `_wsetlocale`, `_create_locale`,
  `_setmbcp`.** From `api-ms-win-crt-locale` both import only
  `___lc_codepage_func`, `___mb_cur_max_func`, `localeconv`; the launcher
  also `_configthreadlocale`, referenced by mingw-w64's wide start-up object
  `crt2u.o` (`llvm-nm`), which picks per-thread versus global locale and
  sets none.
- `isdigit`, `isspace`, `isxdigit`, `tolower`, `atof`, `strtod`,
  `mbrtowc`, `wcrtomb`, `localeconv` come from libc++ and SDL3, not from
  our objects: an `llvm-nm` scan of every object under
  `build/CMakeFiles/bof3x.dir` and `bof3x_launcher.dir` finds ours
  referencing only `_stricmp` (`crt_rest.cpp`), `_strnicmp` (`mp3.cpp`),
  `lstrcmpiA` (`dat_cache.cpp`, `dat_load.cpp`, `music_seq.cpp`),
  `CompareStringA` (`dat_cache.cpp`), `strtod` (`win_main.cpp`), `strtof`
  (`satpixie.cpp`, `config.cpp`). SDL3's own: `atof` (`SDL_string.c`),
  `GetLocaleInfoA` (`SDL_syslocale.c`, which reads the user's languages and
  sets nothing).
- **A trap for later:** `libc++.a`'s `locale.cpp`, `locale_win32.cpp` and
  `iostream.cpp` reference `setlocale` (`llvm-nm`); they are not linked
  today. Pulling in `<iostream>` or a `std::locale` facet would link them,
  and libc++'s Windows locale guard calls `setlocale` while it works. After
  any change that adds a C++ stream or locale, re-run the import check:
  `llvm-readobj --coff-imports build/bof3x.dll | grep -i setlocale` must
  print nothing.

### 2.3 Windows `A` calls on paths and text

Player-chosen paths are the install directory, the folder the launcher and
DLL sit in, and the cache folder. Each `A` call, by what it is given:

| Where | Call | Given | Outside the code page |
|---|---|---|---|
| `src/launcher/launcher.cpp` 216, 78, 89, 293 | `GetEnvironmentVariableW`, `GetModuleFileNameW`, `CreateFileW`, `CreateProcessW` | `BOF3_GAME_DIR`, `--game`, the launcher's folder, `BOF3.exe` | wide: safe |
| `src/launcher/config.cpp` 35, 54, 17 | `CreateFileW`, `FindFirstFileW` | `bof3x.ini`, `BOF3.CFG`, `DAT\` patterns | wide: safe |
| `src/game/crt_rest.cpp` 249, 266 (`Crt_findfirst`, `Crt_fopen`); `src/game/file_io.cpp` 46, 85; `src/game/dat_load.cpp` 118, 344 | `_findfirst32`, `fopen`, `FindFirstFileA` | the game's relative paths (`DAT\`, `BGM\`, `SND\`, saves, `BOF3.CFG`), `DAT\<layer>.*.DAT` | relative: safe (probe); `File_CdRoot` is a drive root (`GetDriveTypeA`, `shell.cpp` 171) |
| `src/hook/log.cpp` 31, `src/hook/crash.cpp` 264, `src/hook/calltrace.cpp` 368, `src/game/win_main.cpp` 170, 180, 194, 276 | `GetModuleFileNameW`, `CreateFileW`, `_wfopen` | `bof3x.log`, crash dumps, `bof3x.window`, F11 frames, beside the DLL | wide: safe. **Their log lines were not** - section 3.1 |
| `src/hook/input_script.cpp` 687 | `GetEnvironmentVariableW` | `BOF3X_SHOT_DIR` | wide: safe; log line section 3.1 |
| `src/hook/crash.cpp` 87 | `GetModuleFileNameA` | a faulting module's name in the crash report | `?` in the name; the report still written. Left: cosmetic |
| `src/game/music_seq.cpp` 308, 316, 325, 47, 122, 147, 186; `src/game/dat_cache.cpp` 363, 372, 52, 60, 69, 384; `src/launcher/config_dialog.cpp` 88, 94 | `GetEnvironmentVariableA`, `GetFileAttributesA`, `CreateFileA`, `FindFirstFileA` | `BOF3X_CACHE` and every path built on it; the game's file layer is also given those paths | **ANSI by design** (DIV-0087): the dialog refuses such a path (`config_dialog.cpp` 85, 361); set by hand in the environment, the `?` fails `GetFileAttributesA` and the DLL stops with `BOF3X_CACHE=... is not a directory` - loud, never silent. Its length limits are in ANSI bytes, so a double-byte path reaches them at half the characters. Section 4 |
| `src/launcher/config.cpp` 326 | `SetEnvironmentVariableA` | `cache=` as the ini has it | the ini's bytes are ACP (the dialog's `ToAnsi`), the env block is wide, the DLL reads it back through the same ACP: consistent |
| `src/launcher/config.cpp` 300, 319, 328, 343, 358, 360, 372, 381 | `SetEnvironmentVariableA` | language tags, layer names (checked to `[A-Za-z0-9-]`), `seq`/`mp3`, numbers, key and pad lines | ASCII |
| `src/hook/input_script.cpp` 195, 596; `src/hook/statehash.cpp` 71, 105, 139; `src/hook/calltrace.cpp` 323; `src/hook/exe_image.cpp` 31, 61; `src/audio/mp3.cpp` 376, 432, 464, 478, 486 | `fopen`, `CreateFileA`, `FindFirstFileA` | `BOF3X_INPUT`, `BOF3X_RECORD`, `BOF3X_STATEHASH`, `BOF3X_CALLTRACE`, `BOF3X_EXEIMAGE`, `BOF3X_MP3_DUMP`, `BOF3X_MP3_COMPARE`: developer variables | ANSI paths; each fails loudly (`Fatal ... cannot open`) or skips. Left: not player-facing |
| `src/game/win_main.cpp` 506, 524, 539 | `MessageBoxA`, `RegisterClassA`, `CreateWindowExA` | `kInsertDisc`, `kError`, `kTitle` (ours, ASCII), the class `"Bof3"` `0x65DA84` (4 bytes, none `>= 0x80`) | ASCII |
| `src/game/psx_rest.cpp` 294, 307 (`Display_TextOut`, `Display_ErrorBox`) | `TextOutA`, `MessageBoxA` through the game's import slots | the overlay's strings (ours, ASCII; `"Save OK"` `0x65DA54` none `>= 0x80`); the error table `0x66B6A8` entries 0..23 and caption `0x66BC1C` | ASCII: **0** bytes `>= 0x80` over 23 strings, 678 bytes (a byte count over `BOF3.exe`, no text copied) |
| `src/hook/log.cpp` 74 | `MessageBoxA` | `Fatal`'s message | ANSI bytes from the environment round-trip; a UTF-8 path from section 3.1 (one `calltrace` message) shows mangled in the box, whole in the log |
| `src/launcher/config_dialog.cpp` 401 | `SetDlgItemTextA` | a slider's number (`snprintf` of a format) | ASCII |
| `src/game/pad_read.cpp` 241, `src/hook/crash.cpp` 203 | `LoadLibraryA` | `"dinput.dll"`, `"dbghelp.dll"` | ASCII; system search path |
| `src/launcher/launcher.cpp` 59, 277, 311; `config.cpp` 302 | `fwprintf` / `wprintf` to `stderr` / `stdout` | messages naming the game directory or the ini | the C locale's conversion: a path above U+00FF cuts the console line; `Die` shows the same text in a `MessageBoxW`, whole. Left |

### 2.4 Case and ctype

- **No ctype call of ours exists** (`isalpha` .. `toupper`, `std::` or not:
  grep, and the object scan of section 2.2). Our own folds are ASCII by
  construction: `dat_cache.cpp` 123 (`Key`, `a..z` only),
  `dat_load.cpp` 336 (layer names `[A-Za-z0-9-]`).
- **`Crt_stricmp`** (`crt_rest.cpp` 228, the game's `_stricmp` entry
  `0x5C2B40`, bound to the toolchain's in [`crt-rest.md`](crt-rest.md)
  2.3) and **`_strnicmp`** (`mp3.cpp` 423, file names in
  `BOF3X_MP3_DUMP_FILES`): fold `A..Z` only under `"C"` (probe). The
  UCRT takes its ASCII path while no `setlocale` has run in the process -
  and `ucrtbase` is one per process, shared by every module in it. Nothing
  of ours calls `setlocale`; whether any other module loaded into the
  game's process does is section 5.
- **`lstrcmpiA`** (`music_seq.cpp` 166, `"%03u.DAT"` against a listed
  name; `dat_cache.cpp` 531, `dat_load.cpp` 418, 471, the DIV-0089
  self-test): a linguistic compare under the **user's** locale, not the C
  locale. On ASCII file names it is safe: the one well-known exception,
  Turkish `I` / `i`, compares equal under `NORM_IGNORECASE` (probe), and
  the only non-test caller compares digits and `DAT`.
- `CompareStringA(LOCALE_INVARIANT, NORM_IGNORECASE, ...)` (`dat_cache.cpp`
  538): locale-free.

### 2.5 Conversions

- `src/launcher/config_dialog.cpp` 46, 57 (`FromAnsi`, `ToAnsi`):
  `CP_ACP`, stated - the cache path's contract with the DLL (the comment
  at 40). `ToAnsi` refuses a lossy conversion.
- After section 3: `config.cpp` `ConfigCacheDataRoot`, `CP_ACP` (the same
  contract); `log.cpp` `Utf8Path`, `CP_UTF8` (log lines).
- No `mbstowcs` / `wcstombs` / `mbrtowc` of ours. SDL3: we call no window,
  title or file API of it (grep for `SDL_CreateWindow`, `SDL_SetWindowTitle`,
  `SDL_IOFromFile`, `SDL_LoadFile`, `SDL_GetBasePath`); the gamepad's name
  (`SDL_GetGamepadName`, UTF-8) and `SDL_GetError` go into the log as bytes
  (`pad_sdl.cpp` 36, 45, 87).

### 2.6 Numbers

- `strtof`: `config.cpp` 151 (`satpixie.*` in `bof3x.ini`), `satpixie.cpp`
  267 (`BOF3X_SATPIXIE`); written by `config.cpp` 266 (`%g`). `strtod`:
  `win_main.cpp` 489 (`BOF3X_FRAME_MS`). `sscanf` `%f`: `draw_order.cpp` 207
  (`BOF3X_DRAWORDER`). `snprintf` of a float: `config_dialog.cpp` 400. All
  read and write `'.'` under `"C"` (probe) - and the ini round trip is
  only consistent because *both* processes stay in `"C"`: a launcher in a
  `,`-decimal locale would write `2,2` that the DLL reads as `2` and
  refuses. One more reason for the rule of [`crt-startup.md`](crt-startup.md)
  section 5.
- Integers (`strtoul`, `strtol`, `atoi`, `sscanf` `%u` / `%x` / `%ld`,
  `std::to_string`) do not depend on the locale's decimal point or
  grouping. `Crt_atoi` and `Crt_sscanf` are ours ([`crt-rest.md`](crt-rest.md)
  2.7).

## 3. What was fixed

### 3.1 Log lines that named a wide path

`bof3::Log` formats with `vsnprintf`; a `%ls` argument goes through the
C locale's wide-to-narrow conversion, which fails above U+00FF (probe), so
`vsnprintf` answered -1 and `Format` logged an empty line. Five lines
named a path beside the DLL or the shot folder that way: the F11 frame
(`win_main.cpp`), the crash dump written / not written (`crash.cpp`, the
two that matter most), the call tracer's `Fatal` (`calltrace.cpp`), and the
recipe shots and their folder (`input_script.cpp`). For a player whose
folder is named in Chinese - the build's own audience - the crash line
vanished.

Fix: `bof3::Utf8Path` (`src/hook/log.h`), a wide path as UTF-8 in a fixed
781-byte buffer (no heap, so the crash reporter's thread may use it), and
the five lines print it with `%s`. No `%ls` is left in the DLL's sources
(grep). The log was already bytes of mixed origin (UTF-8 pad names, ANSI
cache paths); a path is now always readable in it.

### 3.2 The launcher's view of the cache folder

`ConfigCacheDataRoot` (`src/launcher/config.cpp`), which decides whether a
language or optional layer is installed in the cache (DIV-0089), widened
the ini's `cache=` a byte a character - as Latin-1 - while the dialog shows
the same bytes through `CP_ACP` and the DLL receives them through `CP_ACP`.
On a 936 system every non-ASCII cache path, and on 1252 one with a byte in
`0x80..0x9F`, named another folder: the launcher then dropped the ini's
language ("DAT\xx.* is not there; playing the original text") or a layer
the DLL would have found. Fix: `MultiByteToWideChar(CP_ACP)`, the dialog's
own conversion. A host test (`tools/launcher_host/config_tests.cpp`
`TestCacheCodePage`) makes a cache folder named with a character the code
page holds outside Latin-1 (U+20AC here, 0x80 under 1252) and checks both
the root and a layer in it: **50 checks, 0 failures** with the fix; the same
test against the old `config.cpp` fails both.

Built (`cmake --build build`) and self-tested headless from a directory
with no `bof3x.ini` (`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*'`): exit 0,
`inject: 10086 ours`, 0 mismatches.

Neither fix changes what the game does; no ledger entry
([`DIVERGENCE.md`](DIVERGENCE.md) is for game behaviour).

## 4. Left: the cache folder's ANSI limit, and the design for state 3

The cache path is carried as ANSI bytes end to end: ini -> launcher
(`SetEnvironmentVariableA`) -> DLL (`GetEnvironmentVariableA`) -> our I/O
(`dat_cache.cpp`, `music_seq.cpp`, `CreateFileA` / `FindFirstFileA`) **and**
the game's own file layer, which is handed cache paths in its `char`
buffers (`File_Open`, `file_io.cpp`; `Crt_fopen`). Converting to `W` calls
at state 2 means a UTF-8 path through all of those plus the ini's encoding
- some ten sites in four files and the launcher's ini format - for a limit
the dialog already explains. Not done.

At state 3 one line does it: our executable's manifest declares
`<activeCodePage>UTF-8</activeCodePage>` (Windows 10 1903 and later), and
every `A` call and narrow CRT call in the process takes UTF-8, so the DLL's
cache I/O and the game's file layer accept any path unchanged. What has to
come with it:

1. **The launcher** keeps its own ACP (the environment block is wide, so
   `SetEnvironmentVariableA` there and `GetEnvironmentVariableA` in a
   UTF-8 child round-trip the *character*, not the bytes). Its ini then
   stores `cache=` as UTF-8 (`MultiByteToWideChar(CP_UTF8)` in
   `FromAnsi` / `ConfigCacheDataRoot`, `SetEnvironmentVariableW`), and
   `ToAnsi`'s refusal goes - with a rule for reading an existing ini's ANSI
   bytes once (invalid UTF-8 -> read as ACP).
2. **Length limits** (`kRootMax`, the dialog's 226) count UTF-8 bytes, up to
   three per character: a Chinese folder name reaches them at a third of the
   characters. Either long-path opt-in (`longPathAware` in the same manifest)
   or a limit counted in UTF-16 units at the `W` boundary.
3. **The start-up check**: `GetACP() == 65001` logged once, so an older
   Windows that ignored the manifest is visible in `bof3x.log` (it then
   behaves as today, ANSI).
4. Section 2.3's rows that are ASCII stay ASCII; the game's error texts are
   ASCII (0 bytes `>= 0x80`), so `MessageBoxA` shows nothing different.

## 5. Not verified

- **An end-to-end run from a non-ASCII install path or launcher folder.** The
  relative-path claim rests on the probe, not a game run (the owner was
  playing on this machine; headless self-tests only).
- **Another module in the game's process calling `setlocale`.** `ucrtbase`
  is shared process-wide; a GPU driver's user-mode part, an overlay
  (Steam, Discord), an IME or a hook DLL that calls it would switch
  `Crt_stricmp`, `_strnicmp` and the float parsers of section 2.6 to that
  locale. Not observed; not measured. If ever needed, `Crt_stricmp` can fold
  `A..Z` itself (its contract per [`crt-rest.md`](crt-rest.md) 2.3) and the
  parsers can take `_strtof_l` with a `"C"` locale object.
- **A system with the "Beta: Use Unicode UTF-8" setting** (ACP 65001 for
  every process). The cache contract stays consistent (both sides use the
  same ACP); the original runtime's `_setmbcp` (crt-startup section 5) and
  the game itself under it were not run.
- **The launcher dialog test** (`tools/launcher_host/dialog_test.py`) was not
  run: it shows the dialog on the desktop, and the owner was playing. The
  change it would cover (`ConfigCacheDataRoot`) is in the host test.
- The probe ran on ACP 1252 only; 936 (the build's own) was reasoned from
  the code page tables, not measured.
