# bof3x.exe's icon, generated on the player's machine (DIV-0095)

**Status:** IN PROGRESS (verified 2026-10-11 on `tooling/exe-icon`: the self-test against
`make_icon.py`'s `.ico`, every pixel; two headless load-screen runs; not yet seen by
the owner)

The executable's icon is not shipped. It is cut from the game's own data on
the player's machine, the first time the game shows the picture, by the
same reads [`tools/make_icon.py`](../tools/make_icon.py) makes offline - that
script's docstring is the specification of the picture. Exe host only: the
hosted build (`--host bof3`) keeps Capcom's icon from `BOF3.exe`'s `.rsrc`.

## 1. What is generated, from what

Icon 10 of `Menu_DrawBigIcon`'s record table (`0x66367C`, four bytes a record:
u, v, CLUT x byte, CLUT y byte) - the whelp, the owner's choice of
2026-10-11 - read from `.data` as the engine copied it from the player's
`BOF3.exe`. The sprite is 40 x 48 at 4 bits a pixel on texture page `0x1E`
(VRAM x 896, y 256 of the shadow `0x6C9F44`, 1024 x 512 halfwords,
[`asset-loading-path.md`](asset-loading-path.md)); its CLUT is the sixteen
halfwords at (CLUT x & `0xF0`, `0x1E0` + CLUT y) - for the whelp
(u 0, v 48, CLUT 224, 482). A halfword `0x0000` is transparent; any other is
15-bit colour, bit 15 ignored. The 48 rows are cropped to the centre 40
(rows 4..43, the owner's choice).

The sheet is a kind-1 image chunk of `FIRST.DAT`, but the menu's CLUTs come
by the compressed upload path, not as image chunks (`make_icon.py`'s
docstring), so the picture exists only after the game has uploaded both:
the title's load screen and the field menu do.

Sizes, as `make_icon.py` makes them: a whole multiple of 40 by nearest
neighbour; above 40 the largest whole multiple, letterboxed (48 px: 40 with a
4 px transparent border; 256 px: 240 with 8); below 40 (16, 24, 32) Pillow's
Lanczos, reproduced exactly - premultiplied alpha, the separable filter with
its 22-bit integer taps, the floored division back - so the self-test can
compare pixels with the reference.

## 2. When

| Step | Where | When |
|---|---|---|
| Watch | `ExeIcon_Frame` (`src/hook/exe_icon.cpp`), from `Game_WinMain`'s loop after each drawn frame, on the main thread (the window's) | every drawn frame until found: ~500 halfwords read |
| Found | some CLUT entry non-zero, some halfword of the 40 x 48 region non-zero, and the same bytes on two drawn frames running | in the live runs, `Frame_Counter` 500 - the load screen |
| Window icon | `WM_SETICON` `ICON_BIG` (`SM_CXICON`, 32) and `ICON_SMALL` (`SM_CXSMICON`, 16), and `GCLP_HICON` / `GCLP_HICONSM` for Alt-Tab; `CreateIconIndirect` from 32-bit BGRA DIB sections | that frame, every run |
| File icon | a worker thread: `bof3x.exe` copied to `bof3x.icon-work.exe`, `BOF3_ICON` and its images replaced (`BeginUpdateResourceW` / `UpdateResourceW` / `EndUpdateResourceW`: 16, 24, 32, 48 px as 32-bit DIBs with their AND masks, 256 px as a PNG through WIC), checked, then the stamp, then moved to `bof3x.exe.new` | once an install: skipped when `bof3x.icon` names this sheet and `bof3x.exe` is already its `to`, or `bof3x.exe.new` is |
| Install | `InstallGeneratedIcon`, `src/launcher/launcher.cpp`, before it starts `bof3x.exe` | the launcher's next start |

The check before the new file is kept: every section but `.rsrc` and every
header word but the checksum, the image size and `.rsrc`'s size byte for
byte the source's (the timestamp and `.stub` at `0x401000` are what
`DetectHost` reads); `.rsrc` still the last section at its address; the
group and the five images read back (`LoadLibraryExW` as a data file) as
written; the manifest kept.

`bof3x.icon` (`src/common/icon_stamp.h`) holds three SHA-256s: `sheet` (the
record, the CLUT and the region's halfwords), `from` (`bof3x.exe` as
copied) and `to` (the new file as verified). The launcher moves the new file
in only when `bof3x.exe` is still `from` and the new file is `to`; otherwise
it deletes the new file (a stale copy of an older build) and says so on
stderr, and the game writes a fresh one at its next load screen. A running
`bof3x.exe` cannot be replaced: the move fails, the launcher says so, and the
file waits for the next start. Nothing here stops the game: every failure is
one `DIV-0095    icon:` line in `bof3x.log`.

## 3. The self-test

`BOF3X_SELFTEST_ONLY=1` with `BOF3X_ICON_VRAM=<a 1 MB dump of the VRAM
shadow>` and `BOF3X_ICON_REF=<the expected .ico>` (exe host; read after
`.data` is copied): renders the icon from the dump, writes
`bof3x.icon-test.ico` and `bof3x.icon-test.exe` beside the log, decodes both
`.ico` files through WIC and compares every pixel of the five images, runs
the file icon's write and check on the test copy, loads the group with
`LoadImageW`, and builds the two window icons. Any mismatch is a `Fatal`
(exit 3). Without `BOF3X_ICON_VRAM` the self-test is as before. The dump and
the reference are game data: `analysis/icon/` (gitignored), never committed.

## 4. The checks, 2026-10-11

Each from a fresh directory under `build/` holding only `bof3x-launcher.exe`,
`bof3x.dll` and `bof3x.exe`; `<bof3>` the main checkout's `bof3`, `<a>` its
`analysis\icon`.

| Check | Command | Exit | Result |
|---|---|--:|---|
| the icon from the dump | `BOF3X_SELFTEST_ONLY=1 BOF3X_ICON_VRAM=<a>\vram_load.bin BOF3X_ICON_REF=<a>\whelp_centre.ico bof3x-launcher.exe --game <bof3> --no-config` | 0 | "16, 24, 32, 48 and 256 px, all 69696 pixels identical"; the test executable "read back as written ... every other section and header word unchanged"; window icons at 32 and 16 px. Pillow reading the two `.ico` files agrees: 0 differing pixels at each size |
| a wrong reference fails | the same with `BOF3X_ICON_REF=<a>\whelp_top.ico` | 3 | `FATAL: icon self-test: the 16 px image differs from the reference in 256 pixels ...` |
| the rewritten executable runs | `bof3x.icon-test.exe` as `bof3x.exe`; `BOF3X_SELFTEST_ONLY=1 bof3x-launcher.exe --game <bof3> --no-config` | 0 | `host: bof3x.exe, our own executable`, `inject: 10087 ours`; `tools/pe_read.py`: the same six sections, `.rsrc` `0x1528` -> `0x58D4` bytes, `(3, 1..5)`, `(14, 'BOF3_ICON')` 76 bytes, the manifest's 867 |
| the ordinary self-test | `BOF3X_SELFTEST_ONLY=1 bof3x-launcher.exe --game <bof3> --no-config` | 0 | no icon line |
| a stale `bof3x.exe.new` (no stamp) | the self-test with the test executable as `bof3x.exe.new` | 0 | the launcher deleted it and said so; `bof3x.exe` unchanged |
| live, first run | `python tools/input_run.py <dir>\load_hold.txt --out <dir>\shots --launcher <dir>\bof3x-launcher.exe --game <bof3> --host exe --no-front --no-save --minutes 3` (a copy of `<a>\load_hold.txt`: the title to the load list, held 15 s) | 0 | found at `Frame_Counter` 500; window icons set; `bof3x.exe.new` and `bof3x.icon` written; the new file byte-identical to the self-test's `bof3x.icon-test.exe` (sha256 `9ad593f8...`) - so its pixels are the reference's |
| live, second run | the same command again | 0 | the launcher: "bof3x.exe now carries the icon generated from the game's data"; `bof3x.exe`'s sha256 the stamp's `to`; the game ran from it and logged "bof3x.exe already carries this icon" |
| hosted, unchanged | `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*' bof3x-launcher.exe --game <bof3> --no-config --host bof3` | 0 | `host: BOF3.exe ... (hosted)`, `inject: 10087 ours`, 1,068 summary lines all `0 MISMATCHES`, no `DIV-0095` line (built before the `crt_rest` change below, which is a no-op hosted) |

The live runs found a defect of the state 3 build on the way: with
`BOF3X_INPUT` set, `CrtRest_Inject` cloned Capcom's `Rand` for the randlog,
and in `bof3x.exe` (no code of Capcom's mapped) every recipe run ended at
`FATAL: Rand: CloneOriginal(0x005B93D2) in bof3x.exe` before the title. The
copy is read only when `BOF3X_ORIGINAL` leaves `Rand` to Capcom, which the
exe host refuses; it is no longer made there (`src/game/crt_rest.cpp`).

## 5. Not verified

- The owner's eye: the window's title-bar and taskbar icon, Alt-Tab, and
  Explorer's view of `bof3x.exe` after the swap (the runs were headless,
  `--no-front`).
- The field menu as the trigger (only the load screen was run).
- A desktop at another DPI: `SM_CXICON` there is still 32 while the process
  is not DPI-aware ([`state3-build.md`](state3-build.md) 2.1); the sizes
  follow whatever it answers.
