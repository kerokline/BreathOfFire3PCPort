# Breath of Fire III — Install

There are two ways in: **the release**, for playing, and **building from
source**, for working on the project. Both read your own copy of the game;
nothing here or in a release contains game data.

## Playing: the release

1. Download the zip from the [Releases page](https://github.com/kerokline/BreathOfFire3PCPort/releases)
   and unzip it into a folder with a **short path**, e.g. `C:\Games\bof3x`.
   Windows cannot play the opening movie from a path of about 160 characters
   or more.
2. Run `bof3x-launcher.exe`. With no install yet, it opens the **Sources**
   page.
3. Add what you own. Type a path, or use **File...** / **Folder...**, then
   **Add**:
   - the PC game's folder (the one holding `BOF3.exe` and `DAT`);
   - a PlayStation disc as `.cue`/`.bin`: US, Japanese, European (English),
     French or German;
   - a PSP disc as `.iso`: Japanese or European;
   - or a `.zip` holding any of these.

   Each one is checked against the known releases by its files' hashes. If it
   is not one, the page says why in a line (a modified file, an unknown
   release, an image that has lost sectors). The first source builds the
   install; each later one is added to the list and the install is rebuilt
   with it. A build takes three to four minutes.
4. The settings page opens. Pick the language, the look and the rest, then
   **Play**.

Everything is written inside the folder you unzipped: `sources.toml` (your
list), `cache\` and `game\`. Your own files are only read, never changed. To
add or remove a source later, use **Sources...** on the settings page.

**ffmpeg** (optional, recommended): a disc's opening logo movie, the PSP's
sound effects and voices, and the repair of the PC port's logo movie are
converted by your own ffmpeg. Get a Windows build from
<https://ffmpeg.org/download.html#build-windows>, then put it on your `PATH`
or use **Locate ffmpeg.exe...** on the Sources page, and build again. Without
it those pieces are skipped, and the build says which.

**Not supported yet:** `.chd` and `.cso` images (convert them back to
`.cue`/`.bin` or `.iso`), and 2048-byte `.iso` images of a PlayStation disc,
which have lost the disc's audio and movie sectors. Both are in
[`IDEAS.md`](IDEAS.md) (I41).

How the page and the importer fit together, and what was measured:
[`install-sources.md`](install-sources.md). How a release is cut:
[`releases/README.md`](releases/README.md).

## Building from source

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
`build/bof3x.exe`, which must stay side by side. The Sources page works from
a checkout too: it runs `tools/importer.py` on `BOF3X_PYTHON` or the `python`
on `PATH`. To make a release zip from a build, see
[`releases/README.md`](releases/README.md) (`tools/make_release.py`).

### Where things go

The launcher's folder is the **home** (or `BOF3X_HOME`, when set). The
launcher looks for things in this order, and anything you name yourself
always wins over the home's defaults:

| What | Looked for in order |
|---|---|
| The game folder | `--game`, `BOF3_GAME_DIR`, the current folder if it holds `BOF3.exe`, the home's `game\`, then `./bof3` |
| The cache | `BOF3X_CACHE`, the ini's `cache=` (the Cache folder box), the home's `cache\` |
| Settings, log, crash dumps, screenshots | beside the launcher and the DLL, as before |

In a checkout the launcher sits in `build\`, which holds no `game\` or
`cache\`, so every existing default and script keeps the behaviour it had.

## The scripted process

Everything the Sources page does can be done by hand. It is the same
importer, and it is still how the test tooling builds its installs.

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
[`SCAFFOLDING.md`](SCAFFOLDING.md) §4 and
[`launcher-settings.md`](launcher-settings.md).

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
[`importer.md`](importer.md) and
[`launcher-suites.md`](launcher-suites.md).

**From a PlayStation or PSP disc alone**, with no PC files: build the cache
from the disc, lay a game directory from it, and point the launcher at both.
Full steps and what such an install lacks are in
[`disc-only-install.md`](disc-only-install.md):

```bash
python tools/importer.py build --source "path/to/your disc.cue" --lang en-US --out "C:/bof3-cache"
```

```bash
python tools/importer.py install --base --cache "C:/bof3-cache" --game "C:/bof3-game" --lang en-US
```

**The Sources page's own commands**, for a script that wants the same result
as the page (a home folder holding `sources.toml`, `cache\` and `game\`):

```bash
python tools/importer.py identify "path/to/a source"
```

```bash
python tools/importer.py sources --home "C:/Games/bof3x" add "path/to/a source"
```

```bash
python tools/importer.py setup --home "C:/Games/bof3x"
```

The full contract, with the JSON the launcher reads, is
[`install-sources.md`](install-sources.md) section 4.
