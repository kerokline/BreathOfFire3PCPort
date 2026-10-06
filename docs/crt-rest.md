# The C runtime's entry points (platform round, step 3)

**Status:** IN PROGRESS (2026-10-06) - step 3 of
[`platform-layers-plan.md`](platform-layers-plan.md) section 4, on
`phase-3/plat2-crt` from `f18b9d9c`. **14 entries ours**
([`src/game/crt_rest.cpp`](../src/game/crt_rest.cpp), shadow name
`crt_rest`, 10,065 -> 10,079): `Rand` reimplemented exactly, `Crt_sprintf`
over exactly the conversions the game's formats use, and `Crt_strncpy`,
`Crt_stricmp`, `Crt_findfirst`, `Crt_findnext` and the eight entries of the
file layer bound to our toolchain's runtime through thin named functions.
Fuzzed against Capcom's where Capcom's can run before its runtime starts,
against Windows' or the bytes written where it cannot; 0 mismatches, 14
planted bugs all refused (section 4). Six entries left for the cutover, each
with its reason (section 3). Headless only: not yet live-checked (section 6).

The plan's rule (section 2.3 there): the unit of work is the entry the game
calls, not Microsoft's runtime behind it, which is neither decompiled nor
ours to publish. [`platform-read-pass.md`](platform-read-pass.md) section 3
listed seventeen entries; this is that list worked through.

## 1. The entries and who calls them

Callers by an E8 / E9 scan of `BOF3.exe`'s `.text` (capstone-confirmed),
split by where the caller lies: game code (every function of it ours since
round fourteen and the platform round), the MP3 decoder `0x5AB7E0..0x5B8DA0`
(still Capcom's, running), and the runtime itself.

| Entry | What | Game sites | Decoder | CRT | Decision |
|---|---|--:|--:|--:|---|
| `0x5B93D2` `Rand` | `rand` | 745 | - | - | ours, exact (2.1) |
| `0x5B9380` `Crt_sprintf` | `sprintf` | 188 | - | - | ours (2.2) |
| `0x5B9450` `Crt_strncpy` | `strncpy` | 11 | - | 3 | toolchain's (2.3) |
| `0x5C2B40` `Crt_stricmp` (named here) | `_stricmp` | 1 | - | - | toolchain's (2.3) |
| `0x5B979A` `Crt_findfirst` (named here) | `_findfirst` | 1 | - | - | toolchain's (2.3) |
| `0x5B9867` `Crt_findnext` (named here) | `_findnext` | 1 | - | - | toolchain's (2.3) |
| `0x5B9B6D` `Crt_fopen` | `fopen` | 6 | - | - | toolchain's (2.4) |
| `0x5B9993` `Crt_fclose` | `fclose` | 4 | - | 1 (`_fcloseall`) | toolchain's (2.4) |
| `0x5B9D4E` `Crt_fread` | `fread` | 1 | - | - | toolchain's (2.4) |
| `0x5B9E65` `Crt_fwrite` | `fwrite` | 1 | - | - | toolchain's (2.4) |
| `0x5B9F9E` `Crt_fseek` | `fseek` | 1 | - | - | toolchain's (2.4) |
| `0x5C3660` `Crt_fileno` | `_fileno` | 1 | - | - | toolchain's (2.4) |
| `0x5C35D6` `Crt_filelength` | `_filelength` | 1 | - | - | toolchain's (2.4) |
| `0x5B9ADA` `Crt_fgets` | `fgets` | 2 | - | - | toolchain's, with the file layer (2.4) |
| `0x5B9550` `Crt_ftol` (named here) | `_ftol` | 253 | 15 | - | left: ours never calls it (2.5) |
| `0x5B9660` `Crt_malloc` / `0x5B9577` `Crt_free` | | 8 / 8 | 19 / 41 | 19 / 10 | left (2.6) |
| `0x5BAD64` `Crt_GetPtd` | `_getptd` | - | - | 4 | left (2.7) |
| `0x5B9A9B` `Crt_atoi`, `0x5B9AA6` `Crt_sscanf` | | 2, 1 | -, 2 | - | left (2.7) |
| `0x5BA057` | the entry point | - | - | - | the executable's until the cutover |

`Crt_atoi`, `Crt_sscanf` and `Crt_fgets` are not in the read pass's
seventeen: group PW named them for `Cfg_Load` ([`shell.md`](shell.md)).
`fgets` had to come with the file layer (its stream is `fopen`'s); the
other two are in the exit test's remainder (section 5).

## 2. Each entry

### 2.1 `Rand` `0x5B93D2` - reimplemented exactly

`call Crt_GetPtd; ecx = [eax + 0x14]; ecx = ecx * 0x343FD + 0x269EC3;
[eax + 0x14] = ecx; eax = (ecx >> 16) & 0x7FFF`: MSVC 6's generator, its
seed `holdrand` in the per-thread data. **The seed starts at 1**:
`_initptd` `0x5BAD51` is `mov [eax + 0x50], 0x675080; mov dword [eax +
0x14], 1`, called by `_mtinit` `0x5BACFD` (`TlsAlloc`, `calloc(1, 0x74)`,
`TlsSetValue`, then `_initptd` on the main thread's block) and by
`_getptd` `0x5BAD64` when a thread has none yet. There is no `srand` in the
binary. Ours keeps the seed in a static of its own starting at 1, so the
sequence from the first call is the original's.

**Threads.** The per-thread seed would part ours from Capcom's if a second
thread drew: Capcom's would read its own block, ours the one static. No
thread does:

- all 745 E8 sites of `Rand` in `BOF3.exe` are game code - ours; none is in
  the decoder or the runtime;
- `BOF3.exe` imports no thread-creating function (its import table has
  `GetCurrentThreadId` and nothing of `CreateThread`, `_beginthread`,
  `timeSetEvent`, timer queues); our DLL creates two threads, the crash
  reporter's and its self-test watchdog (`hook/crash.cpp`), neither of which
  calls game code;
- in every trace of `analysis/calltrace/platform_1005/*.tsv` (the attract
  sequence and the ten routes, the fourth column the thread) every first
  entry - `Rand`'s, the decoder's `0x5B281C` included - is on the run's one
  thread: 182..199 entries a run, one thread id each. So `0x5B281C`, which
  the read pass took for "the decoder's own thread" (its section 6 marked it
  unread), runs on the main thread: it is entered with caller 0 because it
  is reached through a pointer.

So only the main thread draws, and one static is its seed. Taken.

**The randlog** (`hook/input_script.cpp`, `randlog     frame F rand K`)
used to clone `Rand` and inject a counting copy at its entry - which our
inject now holds. Folded: our `Rand` counts while the randlog is on
(`crt_rest::RandCount_Start`), the line unchanged byte for byte. One
configuration needed more than the brief's "the clone path goes": under
`BOF3X_ORIGINAL` naming `Rand` (or `*`, every reference side), our entry
jumps to Capcom's and ours never runs, so the count would read 0 - the very
regression fixed on 2026-10-05. There the counting copy stays, made by
`CrtRest_Inject` before its inject (`CloneOriginal` refuses after) and put
at Capcom's entry as an instrument only then. Ours and the counter never
patch the same address in one run: ours patches `0x5B93D2` only when on,
the counter only when ours is off. Under `BOF3X_CALLTRACE` there is no count,
as before.

**The map_cells live check** (`BOF3X_SHADOW=map_cells` in game) compares
`Rand`'s seed across its two passes; it read `Crt_GetPtd() + 0x14`, which
our `Rand` no longer touches. It reads `crt_rest::RandSeedCell()` now: ours,
or the per-thread word when `Rand` is left original.

**Reset after the fuzzes.** Every module's start-up fuzz runs before
`CrtRest_Inject` (last in `inject_all.cpp`), and any of ours they run binds
`Rand` by name to ours. The inject sets the seed back to 1 and the count to
0 after them all.

### 2.2 `Crt_sprintf` `0x5B9380` - ours, over the game's formats

The scan (a capstone window before each of the 188 E8 sites, the nearest
`push imm32` of a NUL-terminated string in `.rdata` / `.data`): **182 sites
pass a format with a conversion, 6 pass a plain string** (`0x444119`,
`0x44435A`, `0x45C459`, `0x45C489`, `0x45C66D`, `0x45C6A1`: "?" / ":" / "/" /
"???" at `0x64E31C`, `0x64E320`, `0x653090`, `0x653094`, copied with no
argument). Ours call the same addresses through their callee tables and the
harnesses, plus three literals of their own (`dat_load.cpp` "DAT\%s" and
"DAT\%s.%s", `file_io.cpp` "%s%s").

| Format | Address | Sites | | Format | Address | Sites |
|---|---|--:|---|---|---|--:|
| `%d` | `0x5E10C0` | 38 | | `%02d.%02d` | `0x5F6308` | 2 |
| `%02d.%02dM` | `0x60ABF0` | 2 | | `%02d.%d%d` | `0x61BC74` | 1 |
| `%4d` | `0x64ADD8` | 3 | | `%7d` | `0x64ADDC` | 7 |
| `%2d` | `0x64D3EC` | 14 | | `%3d` | `0x64E324` | 29 |
| `DAT\%s` | `0x652894` | 2 | | `%02d` | `0x65306C` | 4 |
| `%03d` | `0x653074` | 2 | | `%04d` | `0x65307C` | 1 |
| `%d%d%d` | `0x653088` | 2 | | `%d/%d` | `0x6531F4` | 1 |
| `%d/6` | `0x653EB4` | 1 | | `*%2d` | `0x653EC0` | 17 |
| `%02d %02d` | `0x654830` | 2 | | `%s` | `0x654900` | 4 |
| `%5d` | `0x65AB08` | 2 | | `Frame Rate = %d` | `0x65DA5C` | 1 |
| `ID %02X/%02X/%02X` | `0x660C7C` | 1 | | `   /%3d` | `0x6639A8` | 8 |
| `%3d/   ` | `0x6639B0` | 8 | | `%8d` | `0x6639C4` | 2 |
| `%3d/%3d` | `0x6639C8` | 6 | | `%X` | `0x6639D4` | 1 |
| `BISLPS%02X.DAT` | `0x664068` | 4 | | `%3dZ` | `0x6641DC` | 1 |
| `SND\%s.DAT` | `0x666F9C` | 1 | | `BGM\%03dN.DAT` | `0x666FA8` | 1 |
| `BGM\%03d.DAT` | `0x666FB8` | 1 | | `%6d` | `0x66AF34` | 1 |
| `AP%2d` | `0x66AF8C` | 2 | | `%6dZ` | `0x66B4A0` | 2 |
| `%7dZ` | `0x66B4A8` | 2 | | `window vfw handle %d` | `0x66B62C` | 1 |
| `open avivideo!%s%s alias vfw` | `0x66B654` | 1 | | `open avivideo!%s alias vfw` | `0x66B674` | 1 |
| `%X@%X` | `0x66BC24` | 1 | | `%s%s` | `0x66BC30` | 2 |

**The conversions, all of them:** `%d` bare or with a width of 2..8 (`%2d`
`%3d` `%4d` `%5d` `%6d` `%7d` `%8d`), `%d` with the `0` flag (`%02d` `%03d`
`%04d`), `%X` bare and `%02X`, and a bare `%s`. No precision, no `-` / `+` /
space / `#` flag, no size prefix, no `%%`, no float. Ours
(`crt_rest.cpp`) takes `%d` and `%X` with an optional `0` flag and a width
up to 8, and a bare `%s`; any other conversion is a `Fatal` naming the
format (rule 4) - which also guards a format that arrives from somewhere
the scan did not see (a localisation's string, a table). MSVC 6's `_output`
`0x5BA4F3` for those: a negative `%d` is its magnitude as unsigned after a
`-` (so `INT_MIN` prints whole); the width counts the sign; without `0` the
pad is spaces before the sign, with it zeros after; `%X` upper case; a null
`%s` prints `(null)`; the return is the characters written, the NUL not
counted. Under the "C" locale `_output`'s lead-byte test never fires, so a
GBK byte in a format is copied like any other. Ours runs in a few dozen
bytes of stack, where Capcom's took about 0x220 - the task stacks are 16 KB
([`SCAFFOLDING.md`](SCAFFOLDING.md) section 3).

### 2.3 `strncpy`, `_stricmp`, `_findfirst`, `_findnext` - the toolchain's

- **`Crt_strncpy`** `0x5B9450`: MSVC's `strncpy.asm` (byte copies to a
  dword boundary, dwords with the `0x7EFEFEFF` NUL test, the rest of `n`
  zero-filled; `dst` returned) - the C contract, the toolchain's
  `strncpy`. Its three CRT callers (`0x5BF130`, `0x5C05AC`, `0x5C066B`) move
  with the inject; the contract is the same for them.
- **`Crt_stricmp`** `0x5C2B40`: with `__lc_handle[LC_CTYPE]` (`0x7DEC10 +
  8`) 0 - the "C" locale; nothing in the game sets one, and the only
  references to the word are the runtime's - both bytes folded `A..Z` to
  `a..z` and compared unsigned, the answer **-1, 0 or 1** (`sbb al, al; sbb
  al, 0xFF; movsx eax, al`). The toolchain's folds alike and answers the
  difference; ours answers its sign. Sole caller `DInput_EnumJoystick`
  `0x5A9620` (ours, `rest_2h`), testing for 0.
- **`Crt_findfirst`** `0x5B979A` / **`Crt_findnext`** `0x5B9867`: MSVC 6's,
  over `FindFirstFileA` / `FindNextFileA` into its `_finddata_t`. **The
  layouts compared** (by the disassembly's stores and mingw-w64's
  `<io.h>`): MSVC 6 stores `attrib` +0, `time_create` +4, `time_access` +8,
  `time_write` +0xC (each a 32-bit `time_t` from `0x5B992F`), `size` +0x10,
  `name` +0x14, 260 bytes - 0x118 in all, the block `Save_ListFiles`
  reserves. The toolchain's **`_finddata_t` is not that** (its `time_t` is
  64-bit unless `_USE_32BIT_TIME_T`); its **`_finddata32_t` is**, field for
  field - `static_assert`ed in `crt_rest.cpp` - so ours binds
  `_findfirst32` / `_findnext32`. The handle goes only between the two,
  inside `Save_ListFiles` (which never calls `_findclose`: the original's
  leak, kept). The times are local-time conversions in both and nothing
  reads them.

### 2.4 The file layer - the toolchain's, all eight together

`fopen`, `fclose`, `fread`, `fwrite`, `fseek`, `_fileno`, `_filelength`
and `fgets`. **Who opens, uses and closes:** `File_Open` / `File_OpenWrite`
open (modes `rb`, `wb`) into `File_Slots`; `File_Read`, `File_Write`,
`File_Seek`, `File_Size` (`_filelength(_fileno())`) use; `File_Close`
closes (`file_io.cpp`, all ours); `Cfg_Load` (`shell.cpp`, ours) opens
`BOF3.CFG` `rt`, reads two lines with `fgets` and closes. Nothing else
touches a stream: no decoder site, and the one runtime caller,
`_fcloseall` `0x5C09A1` (from the runtime's exit path `0x5BCBCD`), walks the
runtime's own stream table from slot 3, which holds nothing once `fopen` is
ours. Every side is ours, so the layer binds to the toolchain's - **as a
set**: a stream one runtime opened must never reach the other's, so
`fgets` (not in the seventeen) comes along, and **an A/B must name the
eight together** (`BOF3X_ORIGINAL=Crt_fopen` alone would hand Capcom's
`FILE` to our `fread`). `*` switches all of them, so every reference side is
whole. Share mode (`_SH_DENYNO` in MSVC 6's `fopen`), binary and text modes
(`CRLF` to `LF`, `^Z` the end of a text stream) are the same in both.

### 2.5 `_ftol` `0x5B9550` - nothing to rebind

`fnstcw; or ah, 0x0C; fldcw; fistp qword; fldcw` back; `edx:eax` the
truncated `st(0)`. The read pass counted "12 sites of ours" by the address's
immediates in our DLL's code. **None of them is a call on a game path**: in
the built DLL, the 15 immediates of `0x5B9550` in `.text` are all in
self-tests and the fuzz tables' static initialisers (`d3d_rest::SelfTest`
x2, `sound::SelfTest`, `move_cmds::SelfTest`, `battle_items::SelfTest`,
`psx_rest::SelfTest`, the initialisers of `magic_s30_fuzz.cpp`,
`area_w3g_fuzz.cpp`, `area_w4d_fuzz.cpp`, `scenario_harness.cpp` x2 each, and
`FieldFrame_Inject`'s clone table) - each the call a clone of Capcom's makes
"where the original called", which is Capcom's code calling Capcom's. In
`src` the remaining constants are those rows and three named, unused
`kFtol`s (`effect_2a_callees.h`, `field_e2_callees.h`, `sprite_screen.cpp`).
The 44 bodies of ours that convert already inline the sequence, each with its
x87 state as its fuzz showed it. **No shared inline was written** - there
was no call to put it in; folding the 44 copies into one would change 44
bodies to no effect on the exit test, and the brief's condition (each
body's x87 state identical, each group's fuzz at 0) is what they already
meet. `_ftol` stays the executable's, called by the decoder (15 sites), and
is named in `symbols.toml` (register convention, no signature).

### 2.6 `Crt_malloc` / `Crt_free` - left for the cutover

**The decoder pairs the runtime's allocators across entries.**
`Mp3_Create` `0x5ADF00` takes its object from `malloc` (`0x5ADF04`) and five
blocks from the runtime's `calloc` `0x5BA221` (`0x5ADF20..0x5ADF90`, stored
at object +4..+0x14), and `Mp3_Destroy` `0x5B0630` frees object +0x10 and
+0x14 through `Crt_free` (`0x5B071D`, `0x5B072D`); 19 `malloc` and 41 `free`
sites in the decoder in all. `calloc` is not `malloc`: it allocates from the
runtime's heap directly. An inject at `0x5B9577` would hand those blocks to
our toolchain's `free` - a crash on the first track change. And the entry
is shared: an inject moves every caller, the decoder's and the runtime's
own (`fclose`'s buffer, start-up) with ours. So the entries stay Capcom's
while the decoder runs, and ours keep calling them by name (one heap for
everything, as now). **Ours' sites:** `LoadDatFile` and its walk
(`dat_load.cpp`), `Snd_LoadBankFile` (`save_menu.cpp`), `Music_LoadFile` /
`Music_Start` / `Music_Release` (`sound.cpp`), the sample records
(`display_env.cpp`), the CLUT rows (`gfx_clut.cpp`, `Gfx_ClutPixels`,
`Gfx_ConvertRow`), `Font_SetGlyphData` (`gfx_image.cpp`), the teardown's
frees (`psx_rest.cpp`), `TextAdvance_Set`, `TextPairs_Apply`. Every block
ours allocates is freed by ours or never; the decoder only reads
`Music_Data` - so at the cutover, when the decoder is replaced, the pair can
bind to the toolchain's at once.

### 2.7 `Crt_GetPtd`, `Crt_atoi`, `Crt_sscanf` - left

- **`Crt_GetPtd`** `0x5BAD64`: the per-thread data stays the executable's
  until the cutover (the plan). Ours reaches it only from
  `crt_rest::RandSeedCell` when `Rand` is left original (the map_cells live
  check); its four callers otherwise are the runtime's (`errno`'s accessors
  `0x5BCA43` / `0x5BCA4C`, `0x5BE874`) and Capcom's `Rand`.
- **`Crt_sscanf`** `0x5B9AA6`: `Cfg_Load`'s `%d %d` and the decoder's
  `Mp3_MemoryIo` (twice). The entry is the decoder's too, and the frame
  hash counts its engine by characters (HANDOFF, Traps); left with the
  decoder.
- **`Crt_atoi`** `0x5B9A9B` (`Cfg_Load`, two sites): pure, but MSVC 6's
  `atol` wraps on overflow where the toolchain's `atoi` need not - a
  hand-edited `BOF3.CFG` would tell them apart. Not in the seventeen; an
  exact one of ours is the cutover's (or the next round's) to write.

## 3. The two tables

**Taken (14):**

| Entry | Ours | Verified |
|---|---|---|
| `Rand` `0x5B93D2` | the generator, a static seed from 1 | 100,000 rounds + 20,000 from 1 against a copy, 2 controls |
| `Crt_sprintf` `0x5B9380` | `%d` / `%X` with `0` and width <= 8, `%s`; `Fatal` else | 47 formats x 3,000 against Capcom's, 5 controls |
| `Crt_strncpy` `0x5B9450` | toolchain `strncpy` | 60,000 rounds at every alignment, 2 controls |
| `Crt_stricmp` `0x5C2B40` | the sign of toolchain `_stricmp` | 60,000 rounds, 3 controls |
| `Crt_findfirst` `0x5B979A`, `Crt_findnext` `0x5B9867` | toolchain `_findfirst32` / `_findnext32` | layout `static_assert`s; 1,776 matches over 5 patterns against `FindFirstFileA` / `FindNextFileA` |
| `Crt_fopen` `0x5B9B6D`, `Crt_fclose` `0x5B9993`, `Crt_fread` `0x5B9D4E`, `Crt_fwrite` `0x5B9E65`, `Crt_fseek` `0x5B9F9E`, `Crt_fileno` `0x5C3660`, `Crt_filelength` `0x5C35D6`, `Crt_fgets` `0x5B9ADA` | the toolchain's | a written file read back at 400 random offsets, its length, a failed open, `fgets` on CRLF lines (Capcom's cannot run before its runtime) |

**Left for the cutover (6 entries ours still reach):**

| Entry | Ours' callers | Why |
|---|---|---|
| `Crt_malloc` `0x5B9660`, `Crt_free` `0x5B9577` | section 2.6 | the decoder pairs `calloc` with `free`; one heap while it runs |
| `Crt_GetPtd` `0x5BAD64` | `crt_rest::RandSeedCell`, with `Rand` left original | the per-thread data is the executable's until the cutover |
| `Crt_sscanf` `0x5B9AA6` | `Cfg_Load` | the decoder's `Mp3_MemoryIo` uses the same entry |
| `Crt_atoi` `0x5B9A9B` | `Cfg_Load` | MSVC 6 wraps where the toolchain's need not; wants one of ours |
| `Crt_ftol` `0x5B9550` | none on a game path (2.5) | the decoder's; ours inline it |

## 4. Verification

`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=crt_rest`, exit 0, `inject: 10079
ours, 0 left original by BOF3X_ORIGINAL`:

- **`Rand`**: a copy of `0x5B93D2..0x5B93F3` (0x22 bytes; its only transfer
  out, the entry's call, re-aimed at a stand-in answering a 0x74-byte block
  of the fuzz's own), the block's +0x14 and our seed set alike each round:
  100,000 rounds (13 edge seeds, then random), the answer and the seed after
  compared; then 20,000 draws from 1 in a row. 0 mismatches. Controls: the
  multiplier `0x343FE` refused in 99,999 rounds, the shift 15 in 99,998.
- **`sprintf`**: Capcom's own, called at its address before the inject (it
  touches no per-thread data, lock or heap), against ours on the 44 image
  formats in place and ours' 3, 3,000 rounds each (ints from edges -
  `INT_MIN`, `INT_MAX`, every power of ten to 10^8 - and three random
  ranges; strings from a pool with null, empty, GBK and over-wide ones), the
  whole 0x100-byte buffer and the count compared. 141,000 rounds, 0
  mismatches. Controls, each ours' output altered as a bug would alter it:
  zeros before the sign (refused 999), `(null)` as nothing (3,201), `%X` in
  lower case (42,421), one pad short (18,139), the NUL counted (141,000).
- **`strncpy`**: Capcom's against ours on 0x60 bytes of source with NULs a
  quarter of the time, every alignment of source and destination (its dword
  path), `n` small or up to 0x50, the 0x80-byte destination and the return
  compared: 60,000 rounds, 0. Controls: no zero fill (46,468), one byte
  short (56,753).
- **`_stricmp`**: Capcom's against ours over an alphabet with both cases,
  the bytes around `A..Z` / `a..z` (`@ [ \` {`) and four above 0x7F, often a
  case-changed copy: 60,000 rounds, 0. Controls: case-sensitive (20,305),
  the raw difference (35,644), signed bytes (14,636).
- **`_findfirst` / `_findnext`**: ours against `FindFirstFileA` /
  `FindNextFileA` over `BISLPS??.DAT` (the game's pattern, from the image),
  `*`, `DAT\*.DAT`, `*.EXE` and a pattern matching nothing: names, sizes,
  attributes and the end, 1,776 matches, 0.
- **The file layer**: 0x5000 random bytes written `wb` in two `fwrite`
  shapes, read back `rb` at 400 random offsets and lengths (counts and bytes),
  `_filelength(_fileno())`, `fgets` with Cfg_Load's `n` of 0x14 over CRLF
  lines (one longer than 0x13), the end, and an open of nothing: 0.

`BOF3X_SHADOW='*'` narrow and wide: section 7.

## 5. The exit test

The plan's: no call or jump from ours into `0x5B9380..0x5C4000` but the
ones left. Two scans after the work:

- **`src`**, every constant in the range outside comments, fuzz files
  aside: what is left is fuzz rows inside non-fuzz files (`field_frame.cpp`'s
  and `object_kinds.cpp`'s clone tables, `scenario_harness.cpp`'s `_ftol`
  row, `boss_harness_eh.cpp`'s routes), the clone call-site constants
  `kStricmp` (`rest_2h_callees.h`), `kFindFirst` / `kFindNext`
  (`save_menu_callees.h`) and `kCrtSprintf` (`shop_states_callees.h`) that
  only the fuzzes read, the three unused `kFtol`s, the harnesses' `.text`
  bound `0x5C3000`, and the coordinate `0x5C0000` (eleven area and scene
  sites) - none a call.
- **The DLL**, every instruction of `bof3x.dll`'s `.text` with an immediate
  or displacement in the range, by function (`llvm-nm`): outside self-tests,
  fuzz initialisers and the injects only `Crt_malloc` / `Crt_free` by name
  (`WalkDatFile`, `TextAdvance_Set`, `TextPairs_Apply`, `Font_SetGlyphData`,
  `Gfx_ClutPixels`, `Gfx_ConvertRow`; the callee tables of `display_env`,
  `save_menu`, `sound`, `psx_rest` hold them as data),
  `crt_rest::RandSeedCell`'s `Crt_GetPtd`, the harnesses' lookup keys
  (`field_o`'s `Sprintf` pushes `0x5B9380` as a key) and the constants
  above.

Before this step ours called by address, besides the names: `Crt_strncpy`
from `area_w3c.cpp`, `battle_e1.cpp`, `field_e2.cpp`, `rest_2c.cpp` (x2),
`_stricmp` from `rest_2h.cpp`, `_findfirst` / `_findnext` from
`save_menu.cpp`, `Rand` from `field_hidden.cpp`. Each names the function
now, and its fuzz row is in the `OURS` form (the clone calls Capcom's at the
address, ours calls ours by name); the four constants nothing read any more
are gone. The harnesses' `THEIRS` rows for `Rand`, `Crt_sprintf` and
`Crt_strncpy` (`area_harness`, `boss_harness`, `magic_harness`,
`scenario_harness`, and the group fuzzes `area_w1f`, `area_w2e`, `area_w3d`,
`magic_s12`, `scena_sc11`) moved to `OURS`, as a callee that changes hands
does: the harness refuses a `THEIRS` row whose key is not Capcom's code.

**What remains, with its caller:** the left table of section 3.

## 6. Not verified

- **Nothing here has run in game.** The state hash over the attract
  sequence and the ten routes is the live check: `Rand`'s count must equal
  the references' per route (combat 2,017 ... `platform-round.md` section
  6), every DAT, BGM and save read goes through the toolchain's file layer,
  and `Save_ListFiles` through `_findfirst32`. A save **write** is on no
  route but `balioAndSunder_2` (slot 6) - worth running.
- The randlog under `BOF3X_ORIGINAL=Rand` (the copy path) has not run; under
  ours it has not run either.
- Under the frame hash (`BOF3X_CALLTRACE`), the runtime's insides that
  ours no longer enters (`_output`, the stream layer) stop counting on the
  ours side - expected differences against a Capcom reference, not
  regressions; the state hash is the check from round fourteen on.
- `analysis/calltrace/entries_logic.txt` (main checkout, not touched here):
  the fourteen entries want their lines with their extents, as every owned
  start does (HANDOFF, Traps). `Rand` has one (`005B93D2 3A`).

## 7. The `'*'` runs

Recorded below when they finish.
