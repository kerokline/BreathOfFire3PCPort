# Breath of Fire III — Install

**Toolchain:** [llvm-mingw](https://github.com/mstorsjo/llvm-mingw) targeting
i686 (the only supported compiler - no MSVC, no MSYS2), `cmake` >= 3.25,
`ninja`, and `python` >= 3.11. The first configure needs the network: SDL3 is
fetched at a pinned tag and built static. `ffmpeg` on `PATH` is optional; the
importer uses it for the opening movie and for the PSP's sound.

```bash
cmake --preset i686
```

```bash
cmake --build build
```

This produces `build/bof3x-launcher.exe`, `build/bof3x.dll` and
`build/bof3x.exe`, which must stay side by side.

**From the PC port.** Run the launcher and point it at your install:

```bash
build/bof3x-launcher.exe --game "C:/Games/BOF3"
```

The game directory can also come from `BOF3_GAME_DIR`, or the current
directory. The launcher shows its settings dialog first (`--config` forces it,
`--no-config` skips it) and keeps its settings in `bof3x.ini` beside itself;
the game's log is `bof3x.log`, in the same place. It starts our `bof3x.exe`;
`--host bof3` starts your `BOF3.exe` with our DLL injected instead, the way
the project ran before October 2026. Details:
[`docs/SCAFFOLDING.md`](docs/SCAFFOLDING.md) §4 and
[`docs/launcher-settings.md`](docs/launcher-settings.md).

**English and the other languages** come from a PlayStation disc. The importer
builds a cache from your PC install and the disc, and the launcher reads it
(the Cache folder box, or `cache=` in `bof3x.ini`). The preset for the US disc
is below; `pc-plus-eu-en-text`, `pc-plus-fr-text`, `pc-plus-de-text` and
`pc-plus-jp-text` take the other discs:

```bash
python tools/importer.py build --preset pc-plus-us-text --source "C:/Games/BOF3/DAT" --source "C:/Games/BOF3/BOF3.exe" --source "path/to/your US disc.cue" --out "C:/bof3-cache"
```

Then pick the language in the launcher's Language box. Add a PSP disc as one
more `--source` for the PSP's art, music and data as options. See
[`docs/importer.md`](docs/importer.md) and
[`docs/launcher-suites.md`](docs/launcher-suites.md).

**From a PlayStation or PSP disc alone**, with no PC files: build the cache
from the disc, lay a game directory from it, and point the launcher at both.
Full steps and what such an install lacks are in
[`docs/disc-only-install.md`](docs/disc-only-install.md):

```bash
python tools/importer.py build --source "path/to/your disc.cue" --lang en-US --out "C:/bof3-cache"
```

```bash
python tools/importer.py install --base --cache "C:/bof3-cache" --game "C:/bof3-game" --lang en-US
```