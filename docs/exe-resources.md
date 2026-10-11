# `BOF3.exe` beyond `.text` / `.rdata` / `.data` / `.bss`: the resources and the other data directories

**Status:** STABLE - verified 2026-10-11 against the player's `BOF3.exe`
(2,584,576 bytes, sha256 prefix `15984158e0badb4b`, link stamp `0x3ADD0C06`
= 2001-04-18).

Closes the last unread half of
[`platform-layers-plan.md`](platform-layers-plan.md) section 6's "whether
mapping the data sections alone is sufficient": the start-up half is
[`crt-startup.md`](crt-startup.md); this is everything in the executable that
is *not* `.text` / `.rdata` / `.data` / `.bss`. Nothing here is a paste of the
file: names, sizes, ids and addresses only ([`CLAUDE.md`](../CLAUDE.md) rule 1).

**The answer in one line, for section 6:** mapping `.rdata` and `.data` (with
`.bss` zeroed to `0x93D6EC`) is sufficient. The only other section, `.rsrc`,
holds one icon group that only `Game_WinMain`'s `LoadIconA` reads, and that
call is already ours; no other data directory exists but the import table and
the IAT, which state 3's executable replaces with its own. Our own executable
needs to carry an icon of its own; it needs nothing else from the player's
file beyond the two data sections.

## 1. Method

Python's `struct` over the file (no `pefile` on this machine) and `capstone`
over `.text`; the two scripts of the session (`pe_dirs.py`, `pe_imports.py`,
`cs_scan2.py`, scratchpad) parse the DOS and PE headers, the sixteen data
directories, the section table, the resource tree, the import descriptors
and the group-icon record, then sweep every instruction of `.text`
(`0x401000..0x5C3668`, 590,756 instructions, resuming past the 119 bytes
capstone cannot decode) for an immediate or absolute displacement inside
`.rsrc` or through the two resource-API import slots, and every aligned word
of `.rdata` and `.data` for a value inside `.rsrc`. The existing tooling
corroborates: `tools/pe_rich.py --exe` for the header, `symbols.toml`'s
`Game_WinMain` row and `src/game/win_main.cpp` for the call sites.

## 2. The section table

Four sections, `SizeOfImage 0x53F000` (`0x400000..0x93F000`), headers
`0x1000`, section and file alignment both `0x1000`, image base `0x400000`,
subsystem 2 (GUI), `Characteristics 0x10F` (relocations stripped, executable,
line numbers and local symbols stripped, 32-bit), `DllCharacteristics 0`,
checksum 0. The file is `0x277000` bytes: exactly the last section's raw end,
no trailing data (so no overlay, no certificate).

| Section | VA | Virtual size | Raw size | Raw offset | Characteristics |
|---|---|---|---|---|---|
| `.text` | `0x401000` | `0x1C2668` | `0x1C3000` | `0x1000` | `0x60000020` code, execute, read |
| `.rdata` | `0x5C4000` | `0x15D22` | `0x16000` | `0x1C4000` | `0x40000040` initialised data, read |
| `.data` | `0x5DA000` | `0x3636EC` | `0x9C000` | `0x1DA000` | `0xC0000040` initialised data, read, write |
| `.rsrc` | `0x93E000` | `0xCA8` | `0x1000` | `0x276000` | `0x40000040` initialised data, read |

**`.data`'s gap.** Raw `0x9C000` bytes (`0x5DA000..0x676000`) and virtual
`0x3636EC` (to `0x93D6EC`): the `0x2C76EC` bytes between are the `.bss` the
loader zeroes. `base/exe/data.bin` spans exactly the raw part
([`exe-import.md`](exe-import.md) section 1: "`0x9C000` bytes ... from
`0x5DA000` to its raw end `0x676000`"), and `data.toml`'s `bss_end` carries
`0x93D6EC`; `src/hook/exe_image.cpp` already checks the running header against
both and that the gap is zero before the game starts
([`exe-import-engine.md`](exe-import-engine.md) section 4.4). So the importer
covers `.data` in full: bytes for the raw part, a zero fill for the rest.

The headers (`0x400000..0x401000`) hold the DOS stub and a Rich record
(`pe_rich.py`: MSVC 6 builds 8168 / 8797 / 8799 / 9049, read in
[`SHARED_SOURCE.md`](SHARED_SOURCE.md)); nothing in `src/` reads the player's
headers at run time - `exe_image.cpp`'s `LiveData` walks the *running*
module's section table, which in state 3 is our own executable's.

## 3. `.rsrc`

Three entries, one icon, all language 1028 (zh-TW):

| Type | Name / id | Lang | VA | Size (bytes) | What |
|---|---|---|---|---|---|
| `RT_GROUP_ICON` (14) | `"BOF3_ICON"` | 1028 | `0x93EC80` | 34 | the group: two entries |
| `RT_ICON` (3) | 1 | 1028 | `0x93E0F0` | 744 | 32 x 32, 4 bpp (16 colours) |
| `RT_ICON` (3) | 2 | 1028 | `0x93E3D8` | 2,216 | 32 x 32, 8 bpp |

No cursor, bitmap, menu, dialog, string table, accelerator, version
resource or manifest. The sizes are the icon formats' own
(744 = 40 + 64 + 512 + 128; 2,216 = 40 + 1,024 + 1,024 + 128), so the images
are whole and there is nothing hidden in the section: the directory,
`0xCA8` bytes, is the three records plus the name string and padding.

**Who reads it.** Of the resource APIs (`LoadIconA`, `LoadCursorA`,
`LoadStringA`, `FindResourceA` / `ExA`, `LoadResource`, `LockResource`,
`SizeofResource`, `FreeResource`, `LoadBitmapA`, `LoadImageA`, `LoadMenuA`,
`DialogBox*`, `CreateDialog*`, `LoadAcceleratorsA`) the import table holds
only `LoadIconA` (slot `0x5C416C`) and `LoadCursorA` (slot `0x5C417C`), and
the `.text` sweep finds one call through each: `0x4FCBAC` and `0x4FCBBC`,
both inside `Game_WinMain` `0x4FCB00` (`symbols.toml`'s row; the only push of
the name string `0x65DA8C` is at `0x4FCB88`). `LoadCursorA(NULL, IDC_ARROW)`
reads the system cursor, not the file. No instruction of `.text` carries an
immediate or an absolute displacement inside `0x93E000..0x93F000`, and no
aligned word of `.rdata` points there; the one aligned `.data` word that does
(`0x623224`, value `0x93E420`) sits among packed byte tables whose neighbours
are not addresses - a coincidence of bytes, not a pointer. The CRT's start-up
reads no resource ([`crt-startup.md`](crt-startup.md)). So the icon group is
the whole use of the section, and it is read by code that has been ours
since `Game_WinMain` was taken (`src/game/win_main.cpp` line 520,
`LoadIconA(hinstance, "BOF3_ICON")`).

**State 3.** `LoadIconA(hinstance, ...)` resolves against the module whose
`hinstance` is passed; in state 3 that is our executable, and the player's
`.rsrc` is not mapped (nothing else needs it to be). The call is per
resource:

| Resource | State 3 |
|---|---|
| `BOF3_ICON` group and its two images | **our executable owns an icon** - a `RT_GROUP_ICON` in `src/launcher/launcher.rc` (today a manifest and the Config dialog; no icon yet) under the name the window class asks for, or the name changed with it: it is our code on both sides. Drawing the player's icon from their file is possible (`LoadLibraryExA(LOAD_LIBRARY_AS_DATAFILE)` + `LoadIconA`) but buys nothing: the image is Capcom's artwork and the taskbar is ours. Owner's call whether to redraw it or load theirs; neither needs `.rsrc` mapped at `0x93E000`. |
| anything else | nothing: there is nothing else. |

One constant to move with the cutover: `src/game/scenario_harness.cpp`'s
`Readable` bounds the image at `0x93F000` ("the end of `.rsrc`") for guarded
reads; in state 3 the mapped range ends at `.bss`'s end, and the harness is
headless-only anyway. `src/hook/statehash.cpp` stops at `0x93E000` already.

## 4. The other data directories

The optional header's sixteen entries (`e_lfanew 0x108`, the optional header at `0x120`, the directory array at file offset `0x180`, the section table at `0x200`):

| Directory | Present | Where | State 3 |
|---|---|---|---|
| Export | no | | nothing |
| Import | **yes**, `0x5D9378`, `0xB4` bytes | `.rdata` (descriptors, lookup tables, hint/name strings and DLL names all in `.rdata`, `0x5D9378..0x5D9D21`) | our executable imports its own; the descriptors are dead bytes inside the mapped `.rdata` (harmless, unread by `src/`: no reference to `0x5D93..`/`0x5D95..`) |
| Resource | **yes**, `0x93E000`, `0xCA8` | `.rsrc` | section 3 |
| Exception | no (x86-32 uses SEH chains on the stack, not a table) | | nothing |
| Security / certificate | no (and no overlay: the file ends at `.rsrc`'s raw end) | | nothing |
| Base relocation | **no** - confirmed: directory empty and `Characteristics` bit `0x1` set (relocs stripped) | | the premise of state 3 (`/FIXED`, base `0x400000`): reserve the range, map the data at its linked addresses |
| Debug | no | | nothing |
| Architecture, global pointer | no | | nothing |
| TLS | **no** - no callbacks, no index slot in the directory's sense; the runtime's `__tlsindex` `0x672950` is a `TlsAlloc` result at run time (`crt-startup.md` section 2, `_mtinit`), not a static TLS slot | | nothing: the per-thread block is dropped with the runtime |
| Load config | no (so no SafeSEH table, no security cookie, no CFG) | | nothing |
| Bound import | no (every descriptor's time stamp 0: not bound) | | nothing |
| IAT | **yes**, `0x5C4000`, `0x1B4` bytes = 109 dwords | `.rdata`, its first bytes: `0x5C4000..0x5C41B4` | 101 imports in 8 DLLs plus 8 terminators (below) |
| Delay import | no | | nothing |
| COM descriptor | no | | nothing |

**The IAT, by DLL** (descriptor order): DDRAW 2 (`0x5C4000`), DINPUT 1
(`0x5C400C`), DSOUND 1 (ordinal 1, `0x5C4014`), GDI32 5 (`0x5C401C`),
KERNEL32 68 (`0x5C4034..0x5C4144`), USER32 22 (`0x5C4148..0x5C41A0`),
WINMM 1 (`mciSendStringA`, `0x5C41A4`), ole32 1 (`CoInitialize`,
`0x5C41AC`). Cross-check against [`exe-import-engine.md`](exe-import-engine.md)
section 2's 13 "loader's import slots": every one lands on the named import
in this table - `0x5C407C` `GetTickCount`, `0x5C4080` / `84` / `88`
`CreateEventA` / `CloseHandle` / `GetDriveTypeA`, `0x5C40B8` `ExitProcess`,
`0x5C4114` `TerminateProcess`, `0x5C4158` `PostQuitMessage`, `0x5C4164`
`MsgWaitForMultipleObjects`, `0x5C4168` `IntersectRect` (the slot
`gfx_texcache.cpp` tests), `0x5C4174` `MessageBoxA`, `0x5C4020` / `24` / `28`
`SetTextColor` / `SetBkMode` / `TextOutA`. The 13 are a subset of the 101;
the count there is of slots `src/` reads, not of the table, and both agree.
In state 3 the mapped `.rdata` still carries these 109 dwords as the file's
bytes (the lookup-table RVAs the linker left, not addresses), so any
remaining `call [slot]` of ours must have been rebound to our own import by
then - section 2 of that doc already plans it ("state 3's executable imports
its own").

## 5. Not verified

- The icon's appearance (whether the 8-bpp image differs from the 4-bpp one
  beyond depth): not decoded, not needed.
- Whether Windows reads anything of the headers once the data is mapped by
  us rather than loaded: it does not - the range is ours, not a module - but
  the state 3 loader has not been built, so this is reasoning, not a run.
- Which of the 101 imports only original-only code reaches: the plan's
  section 2 measurement, not this doc's.

## 6. Commands

```
python <scratch>/pe_dirs.py    bof3/BOF3.exe <res.pkl>   # headers, directories, sections, resource tree
python <scratch>/pe_imports.py bof3/BOF3.exe <res.pkl>   # import table, IAT span, group-icon entries
python <scratch>/cs_scan2.py   bof3/BOF3.exe             # .text sweep: .rsrc operands, LoadIconA / LoadCursorA slots
python tools/pe_rich.py --exe bof3/BOF3.exe              # the Rich record
```

The three scratch scripts are forty lines each over `struct` and `capstone`;
rerunning them is the verification. They were not added to `tools/` because
the measurement is one-off: the file does not change.
